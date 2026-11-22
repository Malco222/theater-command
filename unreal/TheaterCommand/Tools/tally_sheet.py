"""The tally marks' sheet for the air battle (Battle/AirTally, AirBattleTally.cpp), from the modeller's
(battle-models/air/tally/tally-sheet.png and tally-sheet.json, the art director's check of 20 November).

The game reads it as plain bytes, so it needs no picture reader: Content/Data/Tally/tally-sheet.rgba, each pixel its
red, green, blue and alpha, row by row from the top, at half the modeller's size (64-pixel cells: a 9 cm mark is some
30 pixels across from 3 m off, and the game makes the smaller sizes itself), and tally-sheet.json, the modeller's index
with this sheet's size. Halved over each pixel's alpha, so a mark's edge keeps its own colour; and each see-through pixel
given the colour of the mark's nearest edge within its own cell, so the game's smaller sizes (a plain average of four
pixels, CampaignTextures) do not draw a dark rim round a mark seen from farther off.

    python3 Tools/tally_sheet.py <modeller's tally folder>
"""
import json
import os
import sys

from PIL import Image

HALF_TIMES = 1  # (128-pixel cells to 64)


def halve(px, w, h):
    out = []
    for y in range(h // 2):
        for x in range(w // 2):
            r = g = b = a = 0.0
            for dy in (0, 1):
                for dx in (0, 1):
                    pr, pg, pb, pa = px[(2 * y + dy) * w + 2 * x + dx]
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
            out.append((r / a, g / a, b / a, a / 4.0) if a > 0 else (0.0, 0.0, 0.0, 0.0))
    return out, w // 2, h // 2


def bleed(px, w, h, cell):
    """Each see-through pixel the colour of the nearest pixel of the mark in its own cell (grown out a ring at a time)."""
    rgb = [p[:3] if p[3] >= 128.0 else None for p in px]
    for _ in range(cell):
        grown = list(rgb)
        changed = False
        for y in range(h):
            for x in range(w):
                i = y * w + x
                if rgb[i] is not None:
                    continue
                near = []
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and nx // cell == x // cell and ny // cell == y // cell and rgb[ny * w + nx] is not None:
                        near.append(rgb[ny * w + nx])
                if near:
                    grown[i] = tuple(sum(c[k] for c in near) / len(near) for k in range(3))
                    changed = True
        rgb = grown
        if not changed:
            break
    return [((rgb[i] if rgb[i] is not None else (0.0, 0.0, 0.0)) + (px[i][3],)) if px[i][3] < 128.0 else px[i] for i in range(len(px))]


def main(folder):
    here = os.path.dirname(os.path.abspath(__file__))
    to = os.path.join(here, "..", "Content", "Data", "Tally")
    im = Image.open(os.path.join(folder, "tally-sheet.png")).convert("RGBA")
    w, h = im.size
    px = [tuple(float(c) for c in p) for p in im.get_flattened_data()] if hasattr(im, "get_flattened_data") else [tuple(float(c) for c in p) for p in im.getdata()]
    index = json.load(open(os.path.join(folder, "tally-sheet.json")))
    for _ in range(HALF_TIMES):
        px, w, h = halve(px, w, h)
    px = bleed(px, w, h, index["cell_px"] >> HALF_TIMES)
    with open(os.path.join(to, "tally-sheet.rgba"), "wb") as f:
        f.write(bytes(max(0, min(255, int(round(c)))) for p in px for c in p))
    index["file"] = "tally-sheet.rgba"
    index["width"], index["height"] = w, h
    index["cell_px"] = index["cell_px"] >> HALF_TIMES
    index["made"] = "Tools/tally_sheet.py from the modeller's tally-sheet.png (%dx%d), halved %d time(s): plain RGBA bytes, rows from the top" % (im.size[0], im.size[1], HALF_TIMES)
    with open(os.path.join(to, "tally-sheet.json"), "w") as f:
        json.dump(index, f, indent=1)
        f.write("\n")
    print("tally sheet: %dx%d, %d bytes" % (w, h, w * h * 4))


if __name__ == "__main__":
    main(sys.argv[1])
