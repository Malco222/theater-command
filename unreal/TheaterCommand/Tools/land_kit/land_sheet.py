"""Lays land_picture's panels out on one sheet 1600 pixels wide, a small caption under each and nothing else (Brian's
picture sheets). Usage: python3 land_sheet.py <panel folder> <sheet.png>"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROWS = [
    ["Sherman_a", "Sherman_b", "Sherman_c"],
    ["PanzerIV_a", "PanzerIV_b", "PanzerIV_c"],
    ["Tiger_a", "Tiger_b", "Tiger_c"],
    ["Tiger_d", "Howitzer105", "leFH18"],
]
WIDE = 1600
EDGE = 16
GAP = 12
CAPTION = 30


def main(folder, out):
    captions = {}
    with open(os.path.join(folder, "captions.txt"), encoding="utf-8") as f:
        for line in f:
            name, _, words = line.rstrip("\n").partition("\t")
            captions[name] = words
    across = len(ROWS[0])
    panel_w = (WIDE - 2 * EDGE - (across - 1) * GAP) // across
    panel_h = round(panel_w * 3 / 4)
    high = 2 * EDGE + len(ROWS) * (panel_h + CAPTION) + (len(ROWS) - 1) * GAP
    sheet = Image.new("RGB", (WIDE, high), (240, 237, 230))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
    for r, row in enumerate(ROWS):
        for c, name in enumerate(row):
            x = EDGE + c * (panel_w + GAP)
            y = EDGE + r * (panel_h + CAPTION + GAP)
            panel = Image.open(os.path.join(folder, name + ".ppm")).convert("RGB")
            sheet.paste(panel.resize((panel_w, panel_h), Image.LANCZOS), (x, y))
            draw.text((x + 2, y + panel_h + 7), captions.get(name, name), font=font, fill=(52, 48, 42))
    sheet.save(out, optimize=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
