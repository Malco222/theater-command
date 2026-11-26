"""The letters of an ace's name under his cockpit, for the air battle (Battle/AirTally, AirBattleTally.cpp): one cell a
capital, in the modeller's hand (battle-models/air-next/ACES.md and tools/battle-models/acepanel.py on
claude/project-thread-3vdnfu: DejaVu Sans Bold, the sign painter's plain block capitals, each narrowed to at most 0.72 of
its height but I, J, 1, / and .; even stroke, no outline, shadow or script).

The game reads it as plain bytes, so it needs no picture reader: Content/Data/Tally/name-letters.a8, one byte a pixel,
how much of it is letter (0 none, 255 all), row by row from the top; and name-letters.json, its index: the cells' size
and where the capitals' middle sits in each, the space and gap the hand leaves, and each character's cell (column, row)
and width as painted, in capitals' heights. Every cell holds its letter's ink centred across it and the capitals'
middle on the same row of pixels, so that the game lays every cell alike: a cell's middle at the letter's middle along
the line, its row `mid` on the line. The last cell, BAR, is solid: the white bar a kill German pilots painted on their
rudders, cut from its middle.

Drawn four times larger and shrunk, so the letters' edges are smooth; the smaller copies the game makes itself.

    python3 Tools/name_letters.py
"""
import json
import os

from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"   # (acepanel.FONT, livery.BLOCK)
NARROW = 0.72                # (acepanel.NARROW: a capital's width, of its height, at most)
KEEP = "IJ1/."               # (never narrowed: acepanel.wide)
SPACE = 0.16                 # (acepanel.SPACE: between two letters, of the capitals' height)
WORD = 0.45                  # (acepanel.WORD: a space between words)
CAP = 40                     # (the capitals' height in the sheet, pixels: a 5 cm capital is some 13 pixels high from 3 m)
CELL_W, CELL_H = 56, 80      # (each cell: a narrowed capital is at most 29 pixels wide, an accent or a tail a cap's half
MID = 40                     #  above or below the capitals; the rest kept clear, so the game's smaller copies keep them
                             #  apart); MID the row of the capitals' middle from the cell's top
COLUMNS = 18
TIMES = 4                    # (drawn this many times larger, then shrunk)

LETTERS = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "./()-'"
    # (his name in his own nation's letters: Latin-1's capitals, and Latin Extended-A's that the rules' names use or
    #  their neighbours might: Polish, Czech, Hungarian, Romanian, Turkish, French, Nordic)
    "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞ"
    "ĂĄĆČĎĘĚĞİŁŃŇŐŒŘŚŞŠŢŤŮŰŹŻŽȘȚ"
)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    to = os.path.join(here, "..", "Content", "Data", "Tally")
    big_cap = CAP * TIMES
    probe = ImageFont.truetype(FONT, 400)
    l, t, r, b = probe.getbbox("H", anchor="ls")
    size = 400.0 * big_cap / (b - t)
    font = ImageFont.truetype(FONT, int(round(size)))
    l, t, r, b = font.getbbox("H", anchor="ls")
    cap_px = b - t                                     # (a capital H's height, drawn large)
    mid_from_base = 0.5 * (b + t)                      # (its middle, from the baseline: negative, up)
    rows = (len(LETTERS) + 1 + COLUMNS - 1) // COLUMNS
    w, h = COLUMNS * CELL_W, rows * CELL_H
    sheet = Image.new("L", (w, h), 0)
    cells = []
    for k, ch in enumerate(LETTERS):
        col, row = k % COLUMNS, k // COLUMNS
        pad = 3 * big_cap
        im = Image.new("L", (pad * 2, pad * 2), 0)
        ImageDraw.Draw(im).text((pad, pad), ch, font=font, fill=255, anchor="ls")
        box = im.getbbox()
        if not box:
            raise RuntimeError("no ink for %r" % ch)
        ink_w = (box[2] - box[0]) / cap_px
        k_narrow = 1.0 if ch in KEEP else min(1.0, NARROW / max(ink_w, 1e-6))
        cut = im.crop((box[0], 0, box[2], im.size[1]))
        cut = cut.resize((max(1, int(round(cut.size[0] * k_narrow))), cut.size[1]), Image.LANCZOS)
        cell = Image.new("L", (CELL_W * TIMES, CELL_H * TIMES), 0)
        # (its ink centred across the cell; the capitals' middle on row MID)
        x0 = (CELL_W * TIMES - cut.size[0]) // 2
        y0 = int(round(MID * TIMES - (pad + mid_from_base)))
        top, bottom = box[1] + y0, box[3] + y0
        if x0 < 2 * TIMES or top < 2 * TIMES or bottom > (CELL_H - 2) * TIMES:
            raise RuntimeError("%r does not fit its cell (x %d, from %d to %d)" % (ch, x0, top, bottom))
        cell.paste(cut, (x0, y0))
        sheet.paste(cell.reduce(TIMES), (col * CELL_W, row * CELL_H))
        cells.append([ch, col, row, round(ink_w * k_narrow, 4)])
    k = len(LETTERS)
    col, row = k % COLUMNS, k // COLUMNS
    sheet.paste(255, (col * CELL_W, row * CELL_H, (col + 1) * CELL_W, (row + 1) * CELL_H))
    cells.append(["BAR", col, row, 0.0])
    with open(os.path.join(to, "name-letters.a8"), "wb") as f:
        f.write(sheet.tobytes())
    index = {
        "about": "An ace's name under his cockpit (Tools/name_letters.py): one byte a pixel, rows from the top, how much is "
                 "letter. Each cell its letter's ink centred across it, the capitals' middle on row 'mid' from its top; "
                 "'cap' the capitals' height in pixels; each cell [character, column, row, width as painted in capitals' "
                 "heights]; 'space' and 'gap' the word space and the gap between letters, in capitals' heights. BAR is "
                 "solid, the German pilots' white bar a kill.",
        "file": "name-letters.a8", "width": w, "height": h, "cap": CAP, "cell_w": CELL_W, "cell_h": CELL_H, "mid": MID,
        "space": WORD, "gap": SPACE,
    }
    with open(os.path.join(to, "name-letters.json"), "w", encoding="utf-8") as f:
        text = json.dumps(index, indent=1, ensure_ascii=False)
        # (one cell a line, so the game's own reader and the cloud test's take them alike)
        f.write(text[:-2] + ',\n "cells": [\n' + ",\n".join("  " + json.dumps(c, ensure_ascii=False) for c in cells) + "\n ]\n}\n")
    print("name letters: %d cells, %dx%d, %d bytes" % (len(cells), w, h, w * h))


if __name__ == "__main__":
    main()
