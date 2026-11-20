"""The game's decals for a bomber's record: one sheet with every mark of the art director's panel (tallies.py), each
for a dark finish and for a light one.

    python3 tally_sheet.py [out folder]        (default /mnt/project-files/battle-models/air/tally)

Writes tally-sheet.png (RGBA) and tally-sheet.json. Each mark is drawn in a square cell whose middle is the slot's place
and whose half-width is SPAN times the slot's `radius` (from her file's tally), upright (v up her side): draw the cell
as a quad of that size on her skin at the slot, turned by the slot's lean. A bomb is white with, on a light finish, its
black edge: multiply its colour by its row's tin (tallies.TINS). The kill symbols are in their own colours. Fade a mark
by drawing it at its opacity (tallies.fill: newest 1.0, oldest 0.67) over her skin.

The shapes are the same ones marks.py lays on the models for the review pictures, so the game and the pictures agree.
"""
import json
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import marks  # noqa: E402
import tallies  # noqa: E402

CELL = 128                     # (pixels a cell)
SPAN = 1.25                    # (a cell's half-width, in the mark's own radius: room for its edge)
SS = 4                         # (drawn this many times larger, then shrunk: smooth edges)
TAGS = ["BOMB", "KILL-GER", "KILL-JAP", "KILL-ITA", "KILL-UK", "KILL-SOV", "KILL-USA", "KILL-FRA", "KILL-BLOSSOM", "KILL-PLANE"]
FINISHES = ["dark", "light"]


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def cell(tag, finish):
    """One mark drawn in its cell, RGBA."""
    size = CELL * SS
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    drawn = marks.bomb_mark(finish) if tag == "BOMB" else marks.kill_mark(tag, finish)
    k = size / (2.0 * SPAN)
    for layer in drawn:
        for col, pts in layer:
            if tag == "BOMB" and col == marks.BOMB_YELLOW:
                col = "FFFFFF"                                                   # (white: the game multiplies it by its row's tin)
            dr.polygon([(size / 2 + x * k, size / 2 - y * k) for x, y in pts], fill=rgb(col) + (255,))
    return im.resize((CELL, CELL), Image.LANCZOS)


def main(out):
    os.makedirs(out, exist_ok=True)
    cols = len(FINISHES)
    sheet = Image.new("RGBA", (CELL * cols, CELL * len(TAGS)), (0, 0, 0, 0))
    index = {}
    for r, tag in enumerate(TAGS):
        index[tag] = {}
        for c, finish in enumerate(FINISHES):
            sheet.paste(cell(tag, finish), (c * CELL, r * CELL))
            W, H = sheet.size
            index[tag][finish] = [round(c * CELL / W, 6), round(r * CELL / H, 6), round((c + 1) * CELL / W, 6), round((r + 1) * CELL / H, 6)]
    sheet.save(os.path.join(out, "tally-sheet.png"))
    # (a look at it on the two finishes it is for: olive drab and bare metal)
    proof = Image.new("RGB", (CELL * cols * 2, CELL * len(TAGS)), (0, 0, 0))
    for c, (finish, ground) in enumerate((("dark", "4C4B35"), ("light", "B6B9BB"))):
        bg = Image.new("RGBA", (CELL, CELL * len(TAGS)), rgb(ground) + (255,))
        strip = sheet.crop((c * CELL, 0, (c + 1) * CELL, CELL * len(TAGS)))
        tinted = strip.copy()
        px = tinted.load()
        tin = rgb(tallies.TINS[0])
        for y in range(CELL):                                                    # (the bomb's row tinted, as the game does)
            for x in range(CELL):
                r_, g_, b_, a_ = px[x, y]
                px[x, y] = (r_ * tin[0] // 255, g_ * tin[1] // 255, b_ * tin[2] // 255, a_)
        bg.alpha_composite(tinted)
        proof.paste(bg.convert("RGB"), (c * CELL * 2, 0))
        bg2 = Image.new("RGBA", (CELL, CELL * len(TAGS)), rgb(ground) + (255,))
        faded = tinted.copy()
        faded.putalpha(faded.getchannel("A").point(lambda a: int(a * tallies.FADE)))
        bg2.alpha_composite(faded)
        proof.paste(bg2.convert("RGB"), (c * CELL * 2 + CELL, 0))
    proof.save(os.path.join(out, "tally-sheet-proof.png"))
    with open(os.path.join(out, "tally-sheet.json"), "w") as f:
        json.dump({"file": "tally-sheet.png", "cell_px": CELL, "span": SPAN, "uv": index,
                   "tint": {"BOMB": "multiply by the row's tin", "tins": tallies.TINS},
                   "victims": tallies.RULES["victims"], "own": tallies.OWN,
                   "fade": tallies.RULES["fade"], "finish": tallies.RULES["finish"],
                   "about": "Each mark of a bomber's record (tallies.py; the art director's ruling of 2026-11-20, "
                            "art-director/battles/bomber-tallies-ruling.md) in a square cell, for a dark finish and a light "
                            "one: uv is [u0, v0, u1, v1] with v from the top of the sheet. Draw a cell as a quad on her skin, "
                            "middle at the slot, half-width span times the slot's radius, upright on her side, turned by the "
                            "slot's lean (`hand`). A bomb: multiply by its row's tin. tally-sheet-proof.png shows each on olive "
                            "drab and on bare metal, at full strength and at the oldest mark's fade."}, f, indent=1)
    print("made", os.path.join(out, "tally-sheet.png"), sheet.size)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/mnt/project-files/battle-models/air/tally")
