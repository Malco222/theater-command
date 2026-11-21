"""Lettering by hand: a sign painter's brush, not a typeface.

The art director, 2026-11-20 17:44Z, of the first 24 nose arts: "The lettering is a computer font. The scripts share
one face, the blocks are one clean sans, and every baseline is ruled straight. Letter each name by hand: its own
letterforms, a slight lean, a wandering baseline, thick and thin brush strokes, no two scripts alike, no typeface
anyone could name. Shadows and edges are hard and offset like a brush, never the soft dark halo the letters sit on now."

So every letter here is a skeleton of our own: the strokes a brush makes, in the order and the direction it makes
them, on a capital one unit tall. A name is lettered by a hand of its own (a seed): how far it leans, how wide its
letters stand, how heavy its brush is and how much it swells on the down strokes (or on the broad of its nib), where
its waist is (the bars of A, E, H and the bowls of B, P, R), how far apart its letters stand and how its baseline
wanders, and which of a few forms it gives the letters that sign painters made in more than one way. Then each letter
is shaken a little from its skeleton, so the same letter twice in one name is never quite the same. Every stroke is
swept by a brush that lands, presses, wavers and lifts. Edges are painted round the letters, heavier on their shadow
side (down and to the right); shadows are the letters again, offset down and to the right, hard.

Three hands, as the air thread's list names them: `script` (joined, leaning), `block` (capitals) and `stencil`
(capitals cut through a stencil: an even weight, square ends, and the bridges a stencil leaves). A `crude` hand (about
one painting in four, the art director: "wobblier, fewer colours, plainer letters. Many hands painted these") shakes
more, wanders more and gives its letters their plainest forms.

    Hand(kind, seed, crude=False)
    letter(text, hand, cap_px, fill, edge=None, shadow=None, wave=0.5, bend=0.0) -> RGBA image of the name, cropped
    measure(text, hand) -> how long the name is, in capital heights
"""
import math
import random

import numpy as np
from PIL import Image, ImageDraw, ImageChops


def arc(cx, cy, rx, ry, a0, a1, step=9.0):
    """Points along an ellipse from angle a0 to a1 (degrees, anticlockwise from +x; a1 < a0 goes clockwise)."""
    n = max(2, int(abs(a1 - a0) / step) + 1)
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * k / (n - 1))), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * k / (n - 1))))
            for k in range(n)]


# ---- the alphabets ----------------------------------------------------------------------------------------------------
# Each letter: (its width, its strokes), every stroke a list of points the brush passes through in order, the capital
# one unit tall. `p` is the hand: p.waist (where the bars stand, 0.44 to 0.58), p.bar (the crossbar of A), p.form(ch)
# (which of a letter's forms this hand uses, 0, 1 or 2).
def block_letter(ch, p):
    w = p.waist
    f = p.form(ch)
    if ch == "A":
        if f == 1:                                            # (a flat top, as some painters cut it)
            return 0.76, [[(0.0, 0.0), (0.32, 1.0), (0.44, 1.0), (0.76, 0.0)], [(0.15, p.bar), (0.61, p.bar)]]
        return 0.74, [[(0.0, 0.0), (0.37, 1.0), (0.74, 0.0)], [(0.16, p.bar), (0.58, p.bar)]]
    if ch == "B":
        return 0.62, [[(0.0, 0.0), (0.0, 1.0)],
                      [(0.0, 1.0), (0.32, 1.0)] + arc(0.32, (1 + w) / 2, 0.24, (1 - w) / 2, 90, -90) + [(0.0, w)],
                      [(0.0, w), (0.36, w)] + arc(0.36, w / 2, 0.26 + 0.02 * (w > 0.5), w / 2, 90, -90) + [(0.0, 0.0)]]
    if ch == "C":
        return 0.68, [arc(0.39, 0.5, 0.37, 0.5, 48 - 6 * f, 312 + 6 * f)]
    if ch == "D":
        return 0.68, [[(0.0, 0.0), (0.0, 1.0)], [(0.0, 1.0), (0.24, 1.0)] + arc(0.24, 0.5, 0.44, 0.5, 90, -90) + [(0.0, 0.0)]]
    if ch == "E":
        return 0.56, [[(0.56, 1.0), (0.0, 1.0), (0.0, 0.0), (0.56, 0.0)], [(0.0, w), (0.44 - 0.06 * f, w)]]
    if ch == "F":
        return 0.54, [[(0.54, 1.0), (0.0, 1.0), (0.0, 0.0)], [(0.0, w), (0.42 - 0.05 * f, w)]]
    if ch == "G":
        if f == 1:                                            # (with a spur)
            return 0.72, [arc(0.39, 0.5, 0.37, 0.5, 48, 360), [(0.76, 0.0), (0.76, w * 0.9), (0.46, w * 0.9)]]
        return 0.72, [arc(0.39, 0.5, 0.37, 0.5, 48, 345) + [(0.75, w * 0.9), (0.46, w * 0.9)]]
    if ch == "H":
        hx = 0.78 if p.kind == "stencil" else 0.66            # (a stencil's H stands wider, so that its cut bar still reads as one)
        return hx, [[(0.0, 0.0), (0.0, 1.0)], [(hx, 0.0), (hx, 1.0)], [(0.0, w), (hx, w)]]
    if ch == "I":
        return 0.0, [[(0.0, 0.0), (0.0, 1.0)]]
    if ch == "J":
        return 0.52, [[(0.52, 1.0), (0.52, 0.28)] + arc(0.26, 0.28, 0.26, 0.28, 0, -180) + [(0.0, 0.36)]]
    if ch == "K":
        if f == 1:                                            # (the arms meeting the stem)
            return 0.62, [[(0.0, 0.0), (0.0, 1.0)], [(0.62, 1.0), (0.0, w * 0.82)], [(0.0, w * 0.82), (0.64, 0.0)]]
        return 0.62, [[(0.0, 0.0), (0.0, 1.0)], [(0.62, 1.0), (0.0, w * 0.76)], [(0.2, w * 1.04), (0.64, 0.0)]]
    if ch == "L":
        return 0.52, [[(0.0, 1.0), (0.0, 0.0), (0.52, 0.0)]]
    if ch == "M":
        if f == 1:                                            # (splayed)
            return 0.86, [[(0.0, 0.0), (0.1, 1.0), (0.43, 0.24), (0.76, 1.0), (0.86, 0.0)]]
        if f == 2:                                            # (its middle short of the line)
            return 0.82, [[(0.0, 0.0), (0.0, 1.0), (0.41, 0.36), (0.82, 1.0), (0.82, 0.0)]]
        return 0.82, [[(0.0, 0.0), (0.02, 1.0), (0.41, 0.0), (0.8, 1.0), (0.82, 0.0)]]
    if ch == "N":
        return 0.66, [[(0.0, 0.0), (0.0, 1.0), (0.66, 0.0), (0.66, 1.0)]]
    if ch == "O":
        return 0.76, [arc(0.38, 0.5, 0.38, 0.5, 90, 452)]
    if ch == "P":
        b = w * 0.92
        return 0.6, [[(0.0, 0.0), (0.0, 1.0), (0.32, 1.0)] + arc(0.32, (1 + b) / 2, 0.27, (1 - b) / 2, 90, -90) + [(0.0, b)]]
    if ch == "Q":
        return 0.76, [arc(0.38, 0.5, 0.38, 0.5, 90, 452), [(0.46, 0.24), (0.84, -0.08)]]
    if ch == "R":
        b = w * 0.95
        if f == 1:                                            # (a leg that swings out)
            leg = [(0.26, b), (0.5, b * 0.45), (0.62, 0.06), (0.7, 0.0)]
        else:
            leg = [(0.28, b), (0.64, 0.0)]
        return 0.64, [[(0.0, 0.0), (0.0, 1.0), (0.32, 1.0)] + arc(0.32, (1 + b) / 2, 0.27, (1 - b) / 2, 90, -90) + [(0.0, b)], leg]
    if ch == "S":
        u = 0.5 + (w - 0.5) * 0.6
        return 0.6, [arc(0.3, (1 + u) / 2, 0.27, (1 - u) / 2, 18 - 8 * f, 270) + arc(0.3, u / 2, 0.3, u / 2, 90, -164 - 8 * f)]
    if ch == "T":
        return 0.66, [[(0.0, 1.0), (0.66, 1.0)], [(0.33, 1.0), (0.33, 0.0)]]
    if ch == "U":
        return 0.64, [[(0.0, 1.0), (0.0, 0.3)] + arc(0.32, 0.3, 0.32, 0.3, 180, 360) + [(0.64, 1.0)]]
    if ch == "V":
        return 0.72, [[(0.0, 1.0), (0.36, 0.0), (0.72, 1.0)]]
    if ch == "W":
        if f == 1:                                            # (two Vs crossed)
            return 0.98, [[(0.0, 1.0), (0.24, 0.0), (0.56, 1.0)], [(0.42, 1.0), (0.74, 0.0), (0.98, 1.0)]]
        return 0.98, [[(0.0, 1.0), (0.22, 0.0), (0.49, 0.74), (0.76, 0.0), (0.98, 1.0)]]
    if ch == "X":
        return 0.68, [[(0.0, 1.0), (0.68, 0.0)], [(0.68, 1.0), (0.0, 0.0)]]
    if ch == "Y":
        return 0.7, [[(0.0, 1.0), (0.35, w * 0.9), (0.7, 1.0)], [(0.35, w * 0.9), (0.35, 0.0)]]
    if ch == "Z":
        return 0.64, [[(0.02, 1.0), (0.64, 1.0), (0.0, 0.0), (0.64, 0.0)]]
    if ch == "-":
        return 0.36, [[(0.02, 0.44), (0.34, 0.44)]]
    if ch == "'":
        return 0.05, [[(0.04, 1.0), (0.01, 0.74)]]
    if ch == ".":
        return 0.05, [[(0.025, 0.0), (0.03, 0.06)]]
    if ch == "0":
        return 0.6, [arc(0.3, 0.5, 0.3, 0.5, 90, 452)]
    if ch == "1":
        return 0.34, [[(0.02, 0.78), (0.26, 1.0), (0.26, 0.0)]]
    if ch == "2":
        return 0.6, [arc(0.29, 0.71, 0.28, 0.29, 160, -24) + [(0.0, 0.0), (0.62, 0.0)]]
    if ch == "3":
        return 0.58, [arc(0.28, 0.755, 0.26, 0.245, 150, -90) + arc(0.28, 0.265, 0.3, 0.265, 90, -150)]
    if ch == "4":
        return 0.64, [[(0.46, 0.0), (0.46, 1.0), (0.0, 0.3), (0.66, 0.3)]]
    if ch == "5":
        return 0.58, [[(0.54, 1.0), (0.08, 1.0), (0.04, 0.56)] + arc(0.29, 0.31, 0.3, 0.31, 125, -150)]
    if ch == "6":
        return 0.6, [arc(0.31, 0.5, 0.29, 0.5, 62, 196) + arc(0.3, 0.3, 0.3, 0.3, 180, 540)]
    if ch == "7":
        return 0.6, [[(0.0, 1.0), (0.6, 1.0), (0.18, 0.0)]]
    if ch == "8":
        return 0.6, [arc(0.3, 0.765, 0.24, 0.235, 270, 632), arc(0.3, 0.265, 0.29, 0.265, 90, 452)]
    if ch == "9":
        return 0.6, [arc(0.3, 0.7, 0.29, 0.3, 0, 360) + [(0.59, 0.66), (0.56, 0.28), (0.42, 0.04), (0.12, 0.06)]]
    return None


# Where a stencil leaves its bridges: (x, y, which way the cut runs: 90 a gap across a bar, 0 a gap across a stem[, how
# wide, against a full bridge: H's are narrow, so that its bar reads whole between its stems (the art director, 12:32Z)]). The
# cut sits beside the stroke it parts from, never through it: ("r", X) is just right of the stem at X, ("l", X) just left
# of it, ("b", Y) just below the bar at Y, ("a", Y) just above it; "w" is the hand's waist and "p" its bowls'.
STENCIL_BRIDGES = {
    "A": [(0.37, 0.97, 90)], "B": [(("r", 0.0), 1.0, 90), (("r", 0.0), "w", 90), (("r", 0.0), 0.0, 90)],
    "D": [(("r", 0.0), 1.0, 90), (("r", 0.0), 0.0, 90)], "E": [(("r", 0.0), 1.0, 90), (("r", 0.0), "w", 90), (("r", 0.0), 0.0, 90)],
    "F": [(("r", 0.0), 1.0, 90), (("r", 0.0), "w", 90)], "G": [(0.39, 1.0, 90)],
    "H": [(("r", 0.0), "w", 90, 0.55), (("l", 0.78), "w", 90, 0.55)], "O": [(0.38, 1.0, 90), (0.38, 0.0, 90)], "Q": [(0.38, 1.0, 90), (0.38, 0.0, 90)],
    "P": [(("r", 0.0), 1.0, 90), (("r", 0.0), "p", 90)], "R": [(("r", 0.0), 1.0, 90), (("r", 0.0), "p", 90)],
    "0": [(0.3, 1.0, 90), (0.3, 0.0, 90)], "4": [(("l", 0.46), 0.3, 90)], "6": [(0.3, 0.6, 90)], "8": [(0.3, 1.0, 90), (0.3, 0.0, 90)],
    "9": [(0.3, 0.4, 90)], "T": [(0.33, ("b", 1.0), 0)], "L": [(0.0, ("a", 0.0), 0)],
}
CUT = 0.045                                              # (half a bridge's width, in capital heights)


def script_capital(ch, p):
    """A brush script's capitals, a show-card writer's: a swash in, a stroke or two, often a turn out."""
    f = p.form(ch)
    if ch == "A":
        return 0.7, [[(-0.02, 0.02), (0.08, 0.1), (0.3, 0.7), (0.42, 1.0), (0.5, 0.62), (0.6, 0.12), (0.66, 0.0), (0.74, 0.05)],
                     [(0.1, 0.36), (0.38, 0.4), (0.66, 0.46)]]
    if ch == "B":
        return 0.62, [[(0.2, 1.0), (0.16, 0.5), (0.12, 0.0)],
                      [(-0.06, 0.84), (0.12, 0.99), (0.26, 1.0)] + arc(0.34, 0.77, 0.2, 0.22, 85, -90) + [(0.22, 0.54)]
                      + arc(0.36, 0.28, 0.26, 0.27, 85, -110) + [(0.1, 0.03), (0.02, 0.12)]]
    if ch == "C":
        return 0.66, [[(0.62, 0.82), (0.66, 0.92)] + arc(0.42, 0.5, 0.38, 0.5, 58, 300) + [(0.74, 0.12)]]
    if ch == "D":
        return 0.7, [[(0.18, 1.0), (0.16, 0.5), (0.12, 0.0)],
                     [(-0.08, 0.12), (0.02, 0.0), (0.22, 0.0)] + arc(0.26, 0.5, 0.42, 0.5, -88, 92) + [(0.1, 0.98), (-0.04, 0.88)]]
    if ch == "E":
        return 0.58, [[(0.6, 0.86)] + arc(0.38, 0.76, 0.24, 0.23, 30, 270) + [(0.34, 0.53)] + arc(0.38, 0.27, 0.32, 0.27, 100, 330) + [(0.68, 0.14)]]
    if ch == "F":
        return 0.6, [[(-0.02, 0.86), (0.2, 0.98), (0.5, 0.94), (0.76, 1.02)],
                     [(0.42, 0.97), (0.38, 0.5), (0.3, 0.08), (0.16, -0.02), (0.02, 0.08)], [(0.18, 0.5), (0.54, 0.54)]]
    if ch == "G":
        return 0.74, [[(0.62, 0.82), (0.66, 0.92)] + arc(0.42, 0.5, 0.38, 0.5, 58, 336) + [(0.8, 0.55)], [(0.38, 0.53), (0.64, 0.555), (0.9, 0.54)]]
    if ch == "H":
        return 0.7, [[(-0.02, 0.86), (0.08, 1.0), (0.12, 0.6), (0.08, 0.0)], [(0.64, 1.0), (0.6, 0.5), (0.56, 0.06), (0.66, 0.02)],
                     [(0.0, 0.44), (0.34, 0.5), (0.7, 0.56)]]
    if ch == "I":
        return 0.36, [[(-0.04, 0.88), (0.12, 1.0), (0.3, 0.98), (0.26, 0.5), (0.2, 0.06), (0.06, -0.02), (-0.04, 0.08)]]
    if ch == "J":
        return 0.52, [[(-0.02, 0.88), (0.2, 1.0), (0.54, 1.0)], [(0.44, 1.0), (0.42, 0.4), (0.34, -0.14), (0.2, -0.38), (0.04, -0.3), (0.08, -0.1)]]
    if ch == "K":
        return 0.66, [[(0.14, 1.0), (0.12, 0.5), (0.06, 0.0)], [(0.64, 1.0), (0.34, 0.64), (0.12, 0.46)],
                      [(0.22, 0.52), (0.42, 0.28), (0.58, 0.04), (0.7, 0.02)]]
    if ch == "L":
        return 0.66, [[(0.46, 0.9), (0.38, 1.0), (0.28, 0.9), (0.22, 0.5), (0.14, 0.1), (0.0, 0.02), (0.06, -0.03), (0.3, 0.02), (0.66, 0.0)]]
    if ch == "M":
        return 0.82, [[(0.0, 0.0), (0.08, 0.6), (0.16, 1.0), (0.38, 0.28), (0.6, 1.0), (0.66, 0.4), (0.7, 0.0), (0.8, 0.05)]]
    if ch == "N":
        return 0.74, [[(0.0, 0.0), (0.08, 0.6), (0.14, 1.0), (0.52, 0.0), (0.6, 0.6), (0.66, 1.0), (0.76, 0.96)]]
    if ch == "O":
        return 0.74, [arc(0.38, 0.5, 0.37, 0.5, 100, 470)]
    if ch == "P":
        return 0.6, [[(0.18, 1.0), (0.14, 0.5), (0.08, 0.0)], [(-0.06, 0.84), (0.12, 0.99)] + arc(0.36, 0.74, 0.24, 0.26, 95, -100) + [(0.14, 0.48)]]
    if ch == "Q":
        return 0.76, [arc(0.38, 0.5, 0.37, 0.5, 100, 470), [(0.36, 0.2), (0.56, 0.02), (0.82, -0.06)]]
    if ch == "R":
        return 0.66, [[(0.18, 1.0), (0.14, 0.5), (0.08, 0.0)], [(-0.06, 0.84), (0.12, 0.99)] + arc(0.36, 0.75, 0.24, 0.25, 95, -100) + [(0.14, 0.5)],
                      [(0.28, 0.5), (0.44, 0.26), (0.58, 0.04), (0.72, 0.02)]]
    if ch == "S":
        return 0.62, [[(0.64, 0.9)] + arc(0.38, 0.76, 0.26, 0.24, 25, 270) + arc(0.38, 0.27, 0.3, 0.27, 90, -170) + [(-0.02, 0.2)]]
    if ch == "T":
        return 0.78, [[(-0.06, 0.84), (0.1, 0.98), (0.4, 0.98), (0.7, 0.97), (0.92, 1.04)], [(0.47, 0.98), (0.44, 0.5), (0.4, 0.1), (0.32, 0.0), (0.22, 0.03)]]
    if ch == "U":
        return 0.7, [[(-0.02, 0.9), (0.06, 1.0), (0.1, 0.6)] + arc(0.32, 0.32, 0.23, 0.32, 180, 360) + [(0.58, 0.62), (0.62, 1.0)],
                     [(0.62, 1.0), (0.58, 0.4), (0.6, 0.0), (0.7, 0.06)]]
    if ch == "V":
        return 0.72, [[(-0.02, 0.92), (0.08, 1.0), (0.14, 0.8), (0.3, 0.0), (0.5, 0.6), (0.62, 0.95), (0.72, 1.0)]]
    if ch == "W":
        return 0.9, [[(-0.02, 0.92), (0.07, 1.0), (0.12, 0.75), (0.24, 0.0), (0.44, 0.75), (0.6, 0.0), (0.76, 0.85), (0.88, 1.0)]]
    if ch == "Y":
        return 0.66, [[(-0.02, 0.92), (0.08, 1.0), (0.2, 0.56), (0.36, 0.5), (0.58, 0.96)], [(0.62, 1.0), (0.46, 0.3), (0.3, -0.2), (0.1, -0.4), (-0.02, -0.3)]]
    if ch == "'":
        return 0.06, [[(0.06, 1.0), (0.01, 0.74)]]
    if ch == "-":
        return 0.36, [[(0.02, 0.36), (0.34, 0.4)]]
    return None


def script_small(ch, p):
    """A brush script's small letters, an x-height of 0.6: each joined from the one before (its first stroke begins
    where the last one's ended), the dots and bars laid after. Returns (width, strokes, marks): `marks` are the
    strokes that are not joined (the dot of an i, the bar of a t)."""
    f = p.form(ch)
    rise = [(0.0, 0.1), (0.04, 0.3), (0.1, 0.48), (0.2, 0.58), (0.3, 0.59)]   # (up the bowl's left side to its top)
    bowl = rise + arc(0.21, 0.3, 0.18, 0.28, 50, 410)
    if ch == "a":
        return 0.6, [bowl, [(0.42, 0.6), (0.4, 0.2), (0.42, 0.02), (0.52, 0.03), (0.6, 0.12)]], ()
    if ch == "b":
        return 0.5, [[(0.0, 0.14), (0.14, 0.7), (0.2, 0.98), (0.13, 1.03), (0.08, 0.85), (0.06, 0.4), (0.07, 0.04), (0.2, 0.0), (0.36, 0.15),
                      (0.38, 0.42), (0.28, 0.58), (0.16, 0.5), (0.3, 0.46), (0.5, 0.5)]], ()
    if ch == "c":
        return 0.52, [rise[:-1] + [(0.3, 0.6), (0.36, 0.55)] + arc(0.22, 0.3, 0.18, 0.28, 75, 300) + [(0.42, 0.08), (0.52, 0.14)]], ()
    if ch == "d":
        return 0.58, [bowl, [(0.42, 1.0), (0.39, 0.4), (0.4, 0.03), (0.5, 0.03), (0.58, 0.12)]], ()
    if ch == "e":
        if f == 1:                                            # (an e without its loop, a quick brush's)
            return 0.46, [[(0.0, 0.1), (0.04, 0.12)] + arc(0.22, 0.3, 0.18, 0.28, 80, 330) + [(0.46, 0.12)], [(0.06, 0.32), (0.36, 0.38)]], (1,)
        return 0.46, [[(0.0, 0.12), (0.2, 0.3), (0.34, 0.44), (0.3, 0.58), (0.16, 0.52), (0.08, 0.3), (0.12, 0.06), (0.28, 0.0), (0.46, 0.1)]], ()
    if ch == "f":
        return 0.42, [[(0.0, 0.1), (0.12, 0.5), (0.26, 0.95), (0.34, 1.02), (0.3, 0.9), (0.2, 0.4), (0.14, -0.1), (0.08, -0.38), (0.0, -0.36),
                       (0.04, -0.2), (0.2, 0.0), (0.42, 0.1)], [(0.04, 0.52), (0.36, 0.54)]], (1,)
    if ch == "g":
        return 0.56, [bowl, [(0.42, 0.6), (0.39, 0.0), (0.32, -0.3), (0.18, -0.42), (0.06, -0.32), (0.2, -0.12), (0.42, 0.02), (0.56, 0.1)]], ()
    if ch == "h":
        return 0.56, [[(0.0, 0.12), (0.12, 0.65), (0.18, 0.98), (0.1, 1.02), (0.06, 0.8), (0.04, 0.0)],
                      [(0.05, 0.28), (0.18, 0.55), (0.3, 0.58), (0.36, 0.4), (0.36, 0.06), (0.46, 0.02), (0.56, 0.1)]], ()
    if ch == "i":
        return 0.36, [[(0.0, 0.12), (0.1, 0.4), (0.15, 0.6), (0.12, 0.2), (0.16, 0.02), (0.26, 0.03), (0.36, 0.12)], [(0.17, 0.8), (0.19, 0.85)]], (1,)
    if ch == "j":
        return 0.4, [[(0.0, 0.12), (0.1, 0.4), (0.16, 0.6), (0.14, 0.0), (0.1, -0.3), (0.0, -0.42), (-0.06, -0.32), (0.08, -0.12), (0.28, 0.04),
                      (0.4, 0.12)], [(0.18, 0.8), (0.2, 0.85)]], (1,)
    if ch == "k":
        return 0.52, [[(0.0, 0.12), (0.12, 0.65), (0.18, 0.98), (0.1, 1.02), (0.06, 0.8), (0.04, 0.0)],
                      [(0.06, 0.22), (0.24, 0.5), (0.36, 0.56), (0.34, 0.42), (0.1, 0.3), (0.3, 0.12), (0.4, 0.02), (0.52, 0.08)]], ()
    if ch == "l":
        return 0.34, [[(0.0, 0.12), (0.14, 0.6), (0.22, 0.98), (0.14, 1.03), (0.08, 0.8), (0.06, 0.3), (0.1, 0.02), (0.22, 0.0), (0.34, 0.1)]], ()
    if ch == "m":
        return 0.7, [[(0.0, 0.03), (0.04, 0.4), (0.07, 0.58), (0.06, 0.0)], [(0.06, 0.34), (0.15, 0.56), (0.25, 0.58), (0.29, 0.4), (0.29, 0.0)],
                     [(0.29, 0.34), (0.39, 0.56), (0.49, 0.58), (0.53, 0.4), (0.53, 0.06), (0.61, 0.02), (0.7, 0.1)]], ()
    if ch == "n":
        return 0.5, [[(0.0, 0.03), (0.04, 0.4), (0.07, 0.58), (0.06, 0.0)],
                     [(0.06, 0.34), (0.16, 0.56), (0.28, 0.58), (0.33, 0.4), (0.33, 0.06), (0.41, 0.02), (0.5, 0.1)]], ()
    if ch == "o":
        return 0.5, [rise + arc(0.21, 0.3, 0.18, 0.28, 50, 420) + [(0.38, 0.53), (0.5, 0.56)]], ()
    if ch == "p":
        return 0.48, [[(0.0, 0.12), (0.08, 0.6), (0.06, -0.42)],
                      [(0.06, 0.4), (0.18, 0.58), (0.34, 0.5), (0.36, 0.2), (0.24, 0.02), (0.08, 0.06), (0.3, 0.06), (0.48, 0.12)]], ()
    if ch == "q":
        return 0.52, [bowl, [(0.42, 0.6), (0.4, 0.0), (0.38, -0.42), (0.46, -0.2), (0.52, 0.0)]], ()
    if ch == "r":
        if f == 2:
            # (a writing master's r: up to a point above the others, a deep notch, a short shoulder, then down)
            return 0.52, [[(0.0, 0.1), (0.07, 0.4), (0.13, 0.72), (0.2, 0.48), (0.36, 0.6), (0.36, 0.3), (0.37, 0.05), (0.45, 0.02), (0.52, 0.1)]], ()
        # (a printer's r, and the brush lifted on each side of it (see stands_alone): a joined one reads as a c)
        return 0.4, [[(0.0, 0.52), (0.07, 0.61), (0.07, 0.3), (0.07, 0.0)], [(0.07, 0.4), (0.15, 0.55), (0.27, 0.61), (0.38, 0.56)]], (1,)
    if ch == "s":
        return 0.44, [[(0.0, 0.1), (0.12, 0.4), (0.22, 0.6), (0.3, 0.4), (0.32, 0.2), (0.22, 0.02), (0.08, 0.04), (0.24, 0.04), (0.44, 0.12)]], ()
    if ch == "t":
        return 0.42, [[(0.0, 0.12), (0.12, 0.5), (0.2, 0.86), (0.16, 0.3), (0.18, 0.03), (0.3, 0.03), (0.42, 0.12)], [(0.04, 0.56), (0.36, 0.6)]], (1,)
    if ch == "u":
        return 0.54, [[(0.0, 0.12), (0.06, 0.4), (0.08, 0.58), (0.06, 0.2), (0.12, 0.02), (0.24, 0.04), (0.32, 0.25), (0.35, 0.58)],
                      [(0.35, 0.58), (0.33, 0.2), (0.37, 0.02), (0.45, 0.03), (0.54, 0.12)]], ()
    if ch == "v":
        return 0.48, [[(0.0, 0.12), (0.08, 0.5), (0.1, 0.58), (0.12, 0.3), (0.2, 0.0), (0.3, 0.3), (0.36, 0.56), (0.48, 0.58)]], ()
    if ch == "w":
        return 0.64, [[(0.0, 0.12), (0.06, 0.5), (0.08, 0.58), (0.1, 0.2), (0.16, 0.0), (0.26, 0.3), (0.3, 0.5), (0.34, 0.2), (0.4, 0.0),
                       (0.5, 0.3), (0.54, 0.56), (0.64, 0.58)]], ()
    if ch == "x":
        return 0.5, [[(0.0, 0.12), (0.1, 0.5), (0.2, 0.58), (0.3, 0.3), (0.38, 0.04), (0.5, 0.1)], [(0.42, 0.56), (0.08, 0.02)]], (1,)
    if ch == "y":
        return 0.52, [[(0.0, 0.12), (0.06, 0.4), (0.08, 0.58), (0.06, 0.2), (0.12, 0.02), (0.24, 0.04), (0.32, 0.25), (0.35, 0.58)],
                      [(0.35, 0.58), (0.33, 0.0), (0.26, -0.3), (0.14, -0.42), (0.04, -0.34), (0.16, -0.14), (0.38, 0.02), (0.52, 0.1)]], ()
    if ch == "z":
        return 0.48, [[(0.0, 0.12), (0.06, 0.56), (0.4, 0.58), (0.06, 0.02), (0.36, 0.02), (0.48, 0.1)]], ()
    return None


def stands_alone(ch, hand):
    """Whether the brush comes to this small letter lifted and leaves it lifted: a printer's r stands on its own."""
    return hand.kind == "script" and ch == "r" and hand.form("r") != 2


# ---- a hand ------------------------------------------------------------------------------------------------------------
class Hand:
    """One painter's lettering of one name (`seed`), in a kind of hand: script, block or stencil."""

    def __init__(self, kind, seed, crude=False):
        r = random.Random(seed * 7907 + {"script": 1, "block": 2, "stencil": 3}.get(kind, 0))
        self.kind, self.seed, self.crude = kind, seed, crude
        self.r = r
        if kind == "script":
            self.slant = r.uniform(8.0, 18.0)
            self.wide = r.uniform(0.88, 1.12)
            self.weight = r.uniform(0.085, 0.12)
            self.contrast = r.uniform(0.45, 0.72)
            self.model = r.choice(("press", "press", "nib"))
            self.nib = math.radians(r.uniform(22.0, 48.0))
            self.space = r.uniform(-0.01, 0.03)
            self.shake = 0.016
        elif kind == "stencil":
            self.slant = r.uniform(-1.0, 3.0)
            self.wide = r.uniform(0.84, 0.98)
            self.weight = r.uniform(0.17, 0.205)
            self.contrast = r.uniform(0.0, 0.06)
            self.model = "nib"
            self.nib = math.radians(r.uniform(30.0, 60.0))
            self.space = r.uniform(0.15, 0.22)
            self.shake = 0.006
        else:
            self.slant = r.choice((0.0, r.uniform(-2.0, 3.0), r.uniform(3.0, 9.0)))
            self.wide = r.uniform(0.78, 1.02)
            self.weight = r.uniform(0.13, 0.185)
            self.contrast = r.uniform(0.08, 0.3)
            self.model = "nib"
            self.nib = math.radians(r.uniform(25.0, 65.0))
            self.space = r.uniform(0.11, 0.2)
            self.shake = 0.013
        self.xh = r.uniform(0.86, 1.08)                      # (how tall its small letters stand against its capitals)
        self.waist = r.uniform(0.44, 0.58)
        self.bar = r.uniform(0.28, 0.4)
        self.wander = r.uniform(0.022, 0.05)                 # (the baseline's wander, in capital heights)
        self.wander_f = r.uniform(0.6, 1.4)
        self.tilt = 2.2                                      # (each letter's own lean, degrees either way)
        self.grow = 0.035                                    # (and its own size)
        self.forms = {}
        if crude:
            self.shake *= 2.6
            self.wander *= 2.0
            self.tilt *= 2.4
            self.grow *= 2.2
            self.contrast *= 0.5
            self.space *= r.uniform(0.9, 1.3)

    def form(self, ch):
        """Which of its forms this hand gives a letter (a crude hand, the plainest)."""
        if self.crude:
            return 0
        if ch not in self.forms:
            self.forms[ch] = self.r.choice((0, 0, 1, 1, 2))
        return self.forms[ch]


def glyph(ch, hand):
    """(width, strokes, marks) of a character in a hand, or None for a space or one it does not know."""
    if hand.kind == "script":
        if ch.islower():
            g = script_small(ch, hand)
            if g:
                return g
        g = script_capital(ch, hand) or script_capital(ch.upper(), hand)
        if g:
            return g[0], g[1], ()
        g = block_letter(ch.upper(), hand)
        return (g[0], g[1], ()) if g else None
    g = block_letter(ch.upper(), hand)
    return (g[0], g[1], ()) if g else None


def layout(text, hand, wave=0.5, bend=0.0):
    """The name's strokes, in capital heights (x along it, y up from its baseline): [(points, joined), ...] where
    joined strokes run on from the one before; and its bridges, for a stencil: [(x, y, angle)]."""
    r = random.Random(hand.seed * 31 + 17)
    sl = math.tan(math.radians(hand.slant))
    gap = hand.space + hand.weight * 0.5
    x = 0.0
    out, bridges = [], []
    ph = r.uniform(0.0, 6.28)
    fresh = True                                         # (the first letter of a word: the brush comes to it lifted)
    after_r = False                                      # (the letter before stood alone: a printer's r)
    letters = []
    for i, ch in enumerate(text):
        if ch == " ":
            x += 0.36 * hand.wide + (0.1 if hand.kind == "script" else gap)
            fresh = True
            continue
        g = glyph(ch, hand)
        if g is None:
            continue
        w, strokes, marks = g
        lr = random.Random(hand.seed * 1009 + i * 97 + ord(ch))
        s = 1.0 + lr.uniform(-hand.grow, hand.grow)
        tilt = math.radians(lr.uniform(-hand.tilt, hand.tilt))
        cx = w * hand.wide * s * 0.5
        small = hand.kind == "script" and ch.islower()
        shake = hand.shake * (0.7 if small else 1.0)
        # (the hand's wobble: a smooth sway through the whole letter, so that a curve stays a curve; each stroke of it
        #  also set down a little off where it was meant to go)
        sway = [(lr.uniform(2.0, 5.0), lr.uniform(0.0, 6.28)) for _ in range(4)]
        off = {}

        def lift(u, v, jitter=True, k=0):
            if jitter:
                if k not in off:
                    off[k] = (lr.gauss(0.0, shake * 0.6), lr.gauss(0.0, shake * 0.6))
                u += shake * 1.5 * math.sin(sway[0][0] * v + sway[0][1]) * math.cos(sway[1][0] * u + sway[1][1]) + off[k][0]
                v += shake * 1.5 * math.sin(sway[2][0] * u + sway[2][1]) * math.cos(sway[3][0] * v + sway[3][1]) + off[k][1]
            if small:
                v = v * hand.xh if v <= 0.6 else 0.6 * hand.xh + (v - 0.6)
            u, v = u * hand.wide * s, v * s
            du, dv = u - cx, v - 0.5
            return cx + du * math.cos(tilt) - dv * math.sin(tilt), 0.5 + du * math.sin(tilt) + dv * math.cos(tilt)

        lifted = [[lift(u, v, True, k) for (u, v) in st] for k, st in enumerate(strokes)]
        cuts = []
        if hand.kind == "stencil" and ch.upper() in STENCIL_BRIDGES:
            for br in STENCIL_BRIDGES[ch.upper()]:
                (u, v, a), gw = br[:3], (br[3] if len(br) > 3 else 1.0)
                off = hand.weight * 0.5 + CUT * gw + 0.012
                if isinstance(u, tuple):
                    u = u[1] + (off if u[0] == "r" else -off) / hand.wide
                if v == "w":
                    v = hand.waist
                elif v == "p":
                    v = hand.waist * 0.93
                elif isinstance(v, tuple):
                    v = v[1] + (off if v[0] == "a" else -off)
                cuts.append(lift(u, v, False) + (a + math.degrees(tilt), gw))
        if after_r and not fresh and small:
            # (after a printer's r: this letter's lead-in goes (see the second pass), and it sits so that its top comes as
            #  near the r's flag as an n's would, whatever its shape low down; an s or an a would stand off a word away)
            main_k = [k for k in range(len(strokes)) if k not in marks]
            st0 = strokes[main_k[0]]
            hi = next((j for j, (u, v) in enumerate(st0) if v > 0.3), 0)
            kept = (st0[hi:] if hi < len(st0) - 1 else st0) + [p for k in main_k[1:] for p in strokes[k]]
            high = [u for (u, v) in kept if v > 0.3] or [u for (u, v) in kept]
            x -= (min(high) - 0.06) * hand.wide * s
        letters.append((x, w * hand.wide * s, lifted, marks, ch, fresh, cuts))
        fresh = False
        after_r = stands_alone(ch, hand)
        x += w * hand.wide * s + gap if hand.kind != "script" else w * hand.wide * s + hand.space
        if after_r:
            x += 0.03 * hand.wide + hand.weight * 0.3       # (a little room after a printer's r, or r and n run into an m)
    total = max(x - (gap if hand.kind != "script" else hand.space), 1e-3)
    exit_pt = None
    prev_alone = False
    for (x0, w, strokes_, marks, ch, fresh, cuts) in letters:
        alone = stands_alone(ch, hand)
        after_alone = prev_alone and not fresh
        prev_alone = alone
        if fresh or alone:
            exit_pt = None
        def base(u):
            """How far the baseline has wandered, and the bend that arches the name."""
            t = (x0 + u) / total
            return hand.wander * wave * 2.0 * math.sin(ph + hand.wander_f * 6.28 * t) * math.cos(0.7 * ph + 2.3 * t)

        bridges += [(x0 + u + sl * v, v + base(u), a, gw) for (u, v, a, gw) in cuts]
        joined_here = hand.kind == "script" and ch.islower()
        main = [k for k in range(len(strokes_)) if k not in marks]
        for k, pts in enumerate(strokes_):
            q = [(x0 + u + sl * v, v + base(u)) for (u, v) in pts]
            join = None
            if joined_here and k == main[0] and exit_pt is not None:
                # (the brush runs on from the last letter: from a high exit straight into this letter's first rise)
                if exit_pt[1] > 0.32:
                    hi = next((j for j, (u, v) in enumerate(pts) if v > 0.3), 0)
                    q = q[hi:] if hi < len(q) - 1 else q
                join = exit_pt
            elif joined_here and k == main[0] and after_alone:
                # (after a printer's r the brush comes down onto this letter's top, not up from the baseline)
                hi = next((j for j, (u, v) in enumerate(pts) if v > 0.3), 0)
                q = q[hi:] if hi < len(q) - 1 else q
            if join is not None:
                q = [join, ((join[0] + q[0][0]) / 2, (join[1] + q[0][1]) / 2 - 0.02)] + q
            out.append((q, k in marks))
            if k == main[-1]:
                exit_pt = q[-1] if (hand.kind == "script" and ch.islower() and not alone) else None
    if bend:
        xs = [p[0] for st, _ in out for p in st]
        lo, hi = min(xs), max(xs)
        mid, half = (lo + hi) / 2, max((hi - lo) / 2, 1e-3)
        out = [([(u, v + bend * ((u - mid) / half) ** 2) for u, v in st], m) for st, m in out]
        bridges = [(u, v + bend * ((u - mid) / half) ** 2, a, gw) for u, v, a, gw in bridges]
    return out, bridges, total


def measure(text, hand):
    """How long the name is, in capital heights, its lean included."""
    _, _, total = layout(text, hand)
    return total + math.tan(math.radians(abs(hand.slant))) + hand.weight


# ---- the brush ---------------------------------------------------------------------------------------------------------
def resample(pts, step):
    """The stroke's points every `step` along it."""
    out = [pts[0]]
    for a, b in zip(pts, pts[1:]):
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(d / step))
        for k in range(1, n + 1):
            out.append((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n))
    return out


def chaikin(pts, times=2):
    """Corners eased a little, as a brush takes them (the ends kept)."""
    for _ in range(times):
        if len(pts) < 3:
            return pts
        q = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            q.append((0.78 * a[0] + 0.22 * b[0], 0.78 * a[1] + 0.22 * b[1]))
            q.append((0.22 * a[0] + 0.78 * b[0], 0.22 * a[1] + 0.78 * b[1]))
        q.append(pts[-1])
        pts = q
    return pts


def sweep(pts, hand, scale, rng, grow=0.0, square=False):
    """The brush along a stroke: [(x, y, half width)] in pixels. It lands a little heavy, swells on its down strokes (or
    on the broad of its nib), wavers, and lifts thinner."""
    pts = chaikin(pts, 1 if hand.kind == "stencil" else 2)
    pts = resample(pts, 0.012)
    n = len(pts)
    if n < 2:
        pts = [pts[0], (pts[0][0] + 0.004, pts[0][1] + 0.01)]
        n = 2
    lengths = [0.0]
    for a, b in zip(pts, pts[1:]):
        lengths.append(lengths[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    L = max(lengths[-1], 1e-6)
    ph, fr = rng.uniform(0, 6.28), rng.uniform(1.5, 3.5)
    land = rng.uniform(1.0, 1.18)
    out = []
    for i, (x, y) in enumerate(pts):
        a, b = pts[max(i - 1, 0)], pts[min(i + 1, n - 1)]
        th = math.atan2(b[1] - a[1], b[0] - a[0])
        c = hand.contrast
        if hand.model == "press":
            down = max(0.0, -math.sin(th))
            f = (1.0 - c) + c * down ** 0.8
        else:
            f = (1.0 - c) + c * abs(math.sin(th - hand.nib))
        t = lengths[i] / L
        if square:
            press = 1.0 + 0.03 * math.sin(ph + fr * 6.28 * t)
        else:
            press = (0.82 + (land - 0.82) * min(1.0, t / 0.05)) if t < 0.12 else 1.0
            press *= 1.0 + (0.05 if not hand.crude else 0.12) * math.sin(ph + fr * 6.28 * t)
            if t > 0.8:
                press *= 1.0 - 0.38 * ((t - 0.8) / 0.2) ** 1.6
        hw = 0.5 * hand.weight * f * press * scale + grow
        out.append((x * scale, -y * scale, max(hw, 0.6)))
    return out


def stamp(dr, sw, dx=0.0, dy=0.0, square=False):
    """A swept stroke into a mask: quads between its points, each side offset along the stroke's own bend at that point
    (so that neighbours share their edges and no notch is left on a curve), and a dab of the brush at every point (a
    stencil's ends left square)."""
    n = len(sw)
    norms = []
    for i in range(n):
        a, b = sw[max(i - 1, 0)], sw[min(i + 1, n - 1)]
        vx, vy = b[0] - a[0], b[1] - a[1]
        d = math.hypot(vx, vy)
        norms.append((-vy / d, vx / d) if d > 1e-9 else (norms[-1] if norms else (0.0, 1.0)))
    for i in range(n - 1):
        (x0, y0, h0), (x1, y1, h1) = sw[i], sw[i + 1]
        (ax, ay), (bx, by) = norms[i], norms[i + 1]
        dr.polygon([(x0 + ax * h0 + dx, y0 + ay * h0 + dy), (x1 + bx * h1 + dx, y1 + by * h1 + dy),
                    (x1 - bx * h1 + dx, y1 - by * h1 + dy), (x0 - ax * h0 + dx, y0 - ay * h0 + dy)], fill=255)
    for i, (x, y, h) in enumerate(sw):
        if square and (i == 0 or i == n - 1):
            continue
        dr.ellipse([x - h + dx, y - h + dy, x + h + dx, y + h + dy], fill=255)


def letter(text, hand, cap_px, fill, edge=None, shadow=None, wave=0.5, bend=0.0, keyline=None):
    """The name lettered by `hand`, its capitals `cap_px` tall: an RGBA image cropped to it. `fill`, `edge`, `shadow`:
    (r, g, b). The edge is painted round the letters, heavier on their shadow side; the shadow is the letters again
    (with their edge), offset down and to the right; both hard, as a brush leaves them. `keyline` (r, g, b): a thin
    dark line round the whole of a pale name, about a tenth of its stroke, so that it keeps its edge where it crosses
    the pale grey of her lower nose (the art director, 2026-11-21 13:53Z)."""
    strokes, bridges, total = layout(text, hand, wave, bend)
    rng = random.Random(hand.seed * 131 + 7)
    square = hand.kind == "stencil"
    e = (0.075 if hand.kind == "script" else 0.065) * cap_px * (0.8 if hand.crude else 1.0) if edge else 0.0
    sh = 0.1 * cap_px if shadow else 0.0
    kl = max(1.5, 0.1 * hand.weight * cap_px) if keyline else 0.0
    xs = [p[0] for st, _ in strokes for p in st]
    ys = [p[1] for st, _ in strokes for p in st]
    pad = int(cap_px * 0.45 + e * 2 + sh * 1.5 + kl * 2) + 4
    x0, y1 = min(xs), max(ys)
    W = int((max(xs) - x0) * cap_px) + 2 * pad
    H = int((y1 - min(ys)) * cap_px) + 2 * pad
    ox, oy = pad - x0 * cap_px, pad + y1 * cap_px
    swept = [(sweep(st, hand, cap_px, random.Random(rng.random()), 0.0, square), mark) for st, mark in strokes]
    swept = [([(x + ox, y + oy, h) for x, y, h in sw], mark) for sw, mark in swept]

    def mask(grow=0.0, dx=0.0, dy=0.0, wobble=0.0):
        m = Image.new("L", (W, H), 0)
        dr = ImageDraw.Draw(m)
        for sw, mark in swept:
            jx, jy = (rng.uniform(-wobble, wobble), rng.uniform(-wobble, wobble)) if wobble else (0.0, 0.0)
            stamp(dr, [(x, y, h + grow) for x, y, h in sw], dx + jx, dy + jy, square)
        return m

    def cut(m, dx=0.0, dy=0.0):
        """A stencil's bridges, cut across its letters."""
        if not bridges:
            return m
        dr = ImageDraw.Draw(m)
        reach = hand.weight * cap_px * 0.9 + e * 2 + 2
        for (u, v, a, gw) in bridges:
            g = max(2.0 if gw >= 1.0 else 1.5, 0.045 * cap_px * gw)
            cx, cy = ox + (u + math.tan(math.radians(hand.slant)) * v) * cap_px + dx, oy - v * cap_px + dy
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            px, py = -sa, ca                                  # (across the cut)
            dr.polygon([(cx - ca * reach + px * g, cy + sa * reach + py * g), (cx + ca * reach + px * g, cy - sa * reach + py * g),
                        (cx + ca * reach - px * g, cy - sa * reach - py * g), (cx - ca * reach - px * g, cy + sa * reach - py * g)], fill=0)
        return m

    layers = []
    if keyline:                                           # (under all: round the letters, their edge and their shadow)
        key = mask(e * 1.15 + kl, 0.28 * e, 0.3 * e)
        if shadow:
            key = ImageChops.lighter(key, mask(e * 1.0 + kl, sh, sh * 0.95))
        layers.append((key, keyline))
    if shadow:
        base = mask(e * 1.0, sh, sh * 0.95)
        layers.append((cut(base, sh, sh * 0.95), shadow))
    if edge:
        ring = mask(e, 0.28 * e, 0.3 * e, 0.12 * e)          # (round the letters by hand: heavier down and to the right)
        layers.append((cut(ring), edge))
    layers.append((cut(mask()), fill))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for m, col in layers:
        lay = Image.new("RGBA", (W, H), tuple(col) + (255,))
        lay.putalpha(m)
        out.alpha_composite(lay)
    box = out.getchannel("A").getbbox()
    return out.crop(box) if box else out
