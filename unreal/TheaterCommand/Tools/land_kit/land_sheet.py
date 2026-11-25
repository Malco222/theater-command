"""Lays land_picture's panels out on one sheet 1600 pixels wide, a small caption under each and nothing else (Brian's
picture sheets). Usage: python3 land_sheet.py <panel folder> <sheet.png>

LAND_SHEET_ACROSS=4 lays another sheet's panels four across (three without it), LAND_SHEET_WIDE=3200 makes the sheet
that many pixels wide (its captions grow with it), and LAND_SHEET_ONLY=name,name,... lays those panels alone, in that
order. A sheet named .jpg is saved as a JPEG."""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROWS = [
    ["Sherman_a", "Sherman_b", "Sherman_c"],
    ["PanzerIV_a", "PanzerIV_b", "PanzerIV_c"],
    ["Tiger_a", "Tiger_b", "Tiger_c"],
    ["Tiger_d", "Howitzer105", "leFH18"],
]
WIDE = int(os.environ.get("LAND_SHEET_WIDE", "1600"))
GROW = WIDE / 1600.0
EDGE = round(16 * GROW)
GAP = round(12 * GROW)
CAPTION = round(30 * GROW)


def main(folder, out):
    captions = {}
    with open(os.path.join(folder, "captions.txt"), encoding="utf-8") as f:
        for line in f:
            name, _, words = line.rstrip("\n").partition("\t")
            captions[name] = words
    rows = ROWS
    if any(name not in captions for row in ROWS for name in row):
        # (another sheet's panels, the national marks' (LAND_KIT_MARKS=1): three across in the painter's order)
        names = list(captions)
        if os.environ.get("LAND_SHEET_ONLY"):
            names = [n for n in os.environ["LAND_SHEET_ONLY"].split(",") if n in captions]
        per = int(os.environ.get("LAND_SHEET_ACROSS", "3"))
        rows = [names[i:i + per] for i in range(0, len(names), per)]
    across = len(rows[0])
    panel_w = (WIDE - 2 * EDGE - (across - 1) * GAP) // across
    panel_h = round(panel_w * 3 / 4)
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", round(15 * GROW))

    def wrapped(words):
        """A caption on as many lines as its panel's width needs."""
        lines, line = [], ""
        for word in words.split(" "):
            longer = (line + " " + word).strip()
            if line and font.getlength(longer) > panel_w - 4:
                lines.append(line)
                line = word
            else:
                line = longer
        return lines + [line]

    texts = {name: wrapped(captions.get(name, name)) for row in rows for name in row}
    step = round(19 * GROW)
    caption = CAPTION + step * (max(len(t) for t in texts.values()) - 1)
    high = 2 * EDGE + len(rows) * (panel_h + caption) + (len(rows) - 1) * GAP
    sheet = Image.new("RGB", (WIDE, high), (240, 237, 230))
    draw = ImageDraw.Draw(sheet)
    for r, row in enumerate(rows):
        for c, name in enumerate(row):
            x = EDGE + c * (panel_w + GAP)
            y = EDGE + r * (panel_h + caption + GAP)
            panel = Image.open(os.path.join(folder, name + ".ppm")).convert("RGB")
            sheet.paste(panel.resize((panel_w, panel_h), Image.LANCZOS), (x, y))
            for k, text in enumerate(texts[name]):
                draw.text((x + 2, y + panel_h + round(7 * GROW) + step * k), text, font=font, fill=(52, 48, 42))
    if out.lower().endswith((".jpg", ".jpeg")):
        sheet.save(out, quality=90, optimize=True)
    else:
        sheet.save(out, optimize=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
