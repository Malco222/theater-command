#!/usr/bin/env python3
"""The night fight from above (fx_lamps_test's fx-lamps-night.ppm) as a PNG with its labels written on it.

    python3 Tools/fx_lamps/picture.py <folder>      (reads fx-lamps-night.ppm and fx-lamps-night-labels.txt there,
                                                     writes fx-lamps-night.png beside them; no Pillow: says so, keeps the PPM)

Each line of the labels file is "x y text" in the picture's pixels.
"""
import os
import sys


def main():
    if len(sys.argv) < 2:
        print("picture.py <folder>", file=sys.stderr)
        return 2
    folder = sys.argv[1]
    ppm = os.path.join(folder, "fx-lamps-night.ppm")
    labels = os.path.join(folder, "fx-lamps-night-labels.txt")
    png = os.path.join(folder, "fx-lamps-night.png")
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("no Pillow here: the picture stays " + ppm)
        return 0
    picture = Image.open(ppm).convert("RGB")
    draw = ImageDraw.Draw(picture)
    if os.path.exists(labels):
        with open(labels, encoding="utf-8") as lines:
            for line in lines:
                parts = line.rstrip("\n").split(" ", 2)
                if len(parts) < 3:
                    continue
                x, y = int(float(parts[0])), int(float(parts[1]))
                # (a dark edge under the words, so they read on a lit patch too)
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    draw.text((x + dx, y + dy), parts[2], fill=(0, 0, 0))
                draw.text((x, y), parts[2], fill=(235, 235, 220))
    picture.save(png, optimize=True)
    print("the picture: " + png)
    return 0


if __name__ == "__main__":
    sys.exit(main())
