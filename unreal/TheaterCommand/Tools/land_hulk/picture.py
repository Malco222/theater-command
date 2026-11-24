#!/usr/bin/env python3
# Old wrecks' cloud test's pictures: each PPM the test painted (placement_<recording>, look_<recording>) made a PNG,
# its words (the .txt beside it: "x y text" a line) written on it. Usage: picture.py <out folder>
import os
import sys

from PIL import Image, ImageDraw

out = sys.argv[1]
for name in sorted(os.listdir(out)):
    if not name.endswith(".ppm"):
        continue
    stem = os.path.join(out, name[:-4])
    picture = Image.open(stem + ".ppm").convert("RGB")
    draw = ImageDraw.Draw(picture)
    if os.path.exists(stem + ".txt"):
        with open(stem + ".txt", encoding="utf-8") as words:
            for line in words:
                parts = line.rstrip("\n").split(" ", 2)
                if len(parts) < 3:
                    continue
                x, y, text = int(parts[0]), int(parts[1]), parts[2]
                # (a dark edge under the words, so they read over the field)
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    draw.text((x + dx, y + dy), text, fill=(0, 0, 0))
                draw.text((x, y), text, fill=(255, 255, 255))
    picture.save(stem + ".png")
    print("picture:", stem + ".png")
