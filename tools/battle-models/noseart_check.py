"""noseart_check.py <ids> <out folder> [--module NAME] : look at nose art paintings as the art director will.

For each painting number in <ids> (e.g. 25,26,27 or 25-31) it writes into <out folder>:
  NNN-full.png    the decal at full size (1408 x 864) on olive drab: check the drawing, the hand, the wear
  NNN-graded.png  704 wide under the battle grade (grit-direction.md): check it reads by lightness, not hue
  NNN-glance.png  300 wide under the grade, about as small as the close view shows it: check it reads in a second
and sheet.jpg, every one graded with its number, name and lettering under it. It prints each name's capital height
(the art director asks 20 to 30 cm) and the paints each painting uses.

--module NAME also imports noseart_more.NAME first, so that a painter can look at a file still being worked on
(noseart_more/_wip_*.py: names starting with _ are not loaded by the game until renamed).
"""
import importlib
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import noseart as na  # noqa: E402

OD = (79, 82, 54)                    # (her olive drab, the model's `upper`)


def stats(a):
    a = a.astype(float)
    mx, mn = a.max(2), a.min(2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0.0).mean()
    br = (a[..., 2] - a[..., 0]).mean()
    lum = 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]
    return sat, br, np.percentile(lum, 97) - np.percentile(lum, 3)


def grade(im, sat_to=0.13, br_to=6.0, con_to=175.0):
    """The battle grade as the art director set it: grey the colours, cool them a little, open the contrast."""
    a = np.asarray(im.convert("RGB")).astype(float)
    lum = (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2])[..., None]
    lo, hi = 0.0, 1.0
    for _ in range(18):
        k = (lo + hi) / 2
        s = stats(np.clip(lum + (a - lum) * k, 0, 255))[0]
        lo, hi = (k, hi) if s < sat_to else (lo, k)
    b = lum + (a - lum) * lo
    l2 = 0.299 * b[..., 0] + 0.587 * b[..., 1] + 0.114 * b[..., 2]
    p3, p97 = np.percentile(l2, 3), np.percentile(l2, 97)
    mid = (p3 + p97) / 2
    b = (b - mid) * (con_to / max(p97 - p3, 1.0)) + mid
    shift = br_to - (b[..., 2] - b[..., 0]).mean()
    b[..., 2] += shift / 2
    b[..., 0] -= shift / 2
    return Image.fromarray(np.clip(b, 0, 255).astype(np.uint8))


def on_od(dec):
    panel = Image.new("RGBA", dec.size, OD + (255,))
    panel.alpha_composite(dec)
    return panel.convert("RGB")


def ids_of(text):
    out = []
    for part in text.split(","):
        if "-" in part:
            a, b = part.split("-")
            out += list(range(int(a), int(b) + 1))
        elif part:
            out.append(int(part))
    return out


def main():
    args = [a for a in sys.argv[1:]]
    module = None
    if "--module" in args:
        k = args.index("--module")
        module = args[k + 1]
        del args[k:k + 2]
    ids, out = ids_of(args[0]), args[1]
    os.makedirs(out, exist_ok=True)
    if module:
        importlib.import_module(module if module.startswith("noseart_more.") else "noseart_more." + module)
    tiles = []
    for n in ids:
        if n not in na.PAINTINGS:
            print(n, "NOT PAINTED")
            continue
        d = na.drawing(n)
        paints = sorted({op[1] for op in d.ops if op[0] == "polys"} | {op[1]["fill"] for op in d.ops if op[0] == "text"})
        dec = na.decal(n)
        full = on_od(dec)
        full.save(os.path.join(out, f"{n:03d}-full.png"))
        g = grade(full.resize((704, 432), Image.LANCZOS))
        g.save(os.path.join(out, f"{n:03d}-graded.png"))
        grade(full.resize((300, 184), Image.LANCZOS)).save(os.path.join(out, f"{n:03d}-glance.png"))
        caps = na.LAST_CAPS.get(n)
        flag = "" if caps and 20.0 <= min(caps) and max(caps) <= 30.0 else "  <-- capitals outside 20 to 30 cm"
        print(n, na.BY_ID[n]["name"], "| crude" if na.crude(n) else "", "| caps", caps, "| paints", len(paints), paints, flag)
        tiles.append((n, g))
    if not tiles:
        return
    cols = 3
    tw, th = 704, 432
    rows = (len(tiles) + cols - 1) // cols
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18)
    sheet = Image.new("RGB", (cols * tw + (cols + 1) * 10, rows * (th + 30) + 10), (36, 36, 36))
    dr = ImageDraw.Draw(sheet)
    for i, (n, t) in enumerate(tiles):
        x, y = 10 + (i % cols) * (tw + 10), 10 + (i // cols) * (th + 30)
        sheet.paste(t, (x, y))
        a = na.BY_ID[n]
        dr.text((x + 4, y + th + 4), f"{n}  {a['name']}  ({a['lettering']}){'  crude hand' if na.crude(n) else ''}", fill=(225, 225, 225), font=f)
    sheet.save(os.path.join(out, "sheet.jpg"), quality=88)
    print("sheet", os.path.join(out, "sheet.jpg"))


if __name__ == "__main__":
    main()
