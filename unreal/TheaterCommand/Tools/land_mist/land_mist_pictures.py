#!/usr/bin/env python3
"""The blood mist's pictures, laid out: land_mist_picture's frames (linear light, <name>.f32) graded through the land
battle's own grade (tools/battle-fx/land_grade.py, Normandy's numbers) and cut out around the man, the frame without
the mist above the one with it, one column a moment. A crop enlarged is enlarged by whole pixels (nearest), so what is
seen is what the screen's own pixels hold. With the count, at each distance, of the pixels the mist changes (any
channel by 3 or more of 255, after the grade).

  python3 land_mist_pictures.py <frames folder> <out folder> [shot,shot ...]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "tools", "battle-fx"))
import land_grade as lg  # noqa: E402

W, H = 1920, 1080
TIMES = ["0", "0.05", "0.15", "0.3", "0.6"]
# name, the panels' labels, crop width and height (screen pixels, about the image's middle, where the camera looks),
# whole-pixel enlargement, the picture's title
SHOTS = [
    ("rifle_15", TIMES, 420, 420, 1, "rifle round through the chest, from the side at 15 m (screen pixels as drawn)"),
    ("rifle_40", TIMES, 210, 210, 2, "the same at 40 m (each screen pixel drawn 2 x 2)"),
    ("rifle_100", TIMES, 105, 105, 4, "the same at 100 m (each screen pixel drawn 4 x 4)"),
    ("rifle_300", TIMES, 70, 70, 6, "the same at 300 m (each screen pixel drawn 6 x 6)"),
    ("mg", ["0.05", "0.15", "0.3", "0.6"], 520, 420, 1, "an mg's burst across three men at 20 m (screen pixels as drawn)"),
    ("head", TIMES, 420, 420, 1, "a head strike from the side at 15 m (screen pixels as drawn)"),
    ("facing_15", TIMES, 420, 420, 1, "rifle round through the chest, from the far side at 15 m, the mist toward the eye"),
    ("sky_20", TIMES, 360, 360, 1, "rifle round through the chest, a low eye at 20 m, against the sky"),
    ("woods_40", TIMES, 210, 210, 2, "rifle round through the chest at 40 m against dark woods (each screen pixel 2 x 2)"),
]


def graded(path):
    raw = np.fromfile(path, dtype=np.float32).reshape(H * W, 3).astype(float)
    return np.round(lg.lands_land(raw, **lg.GRADES["normandy"])).clip(0, 255).astype(np.uint8).reshape(H, W, 3)


def crop(img, cw, ch, k):
    x0, y0 = (W - cw) // 2, (H - ch) // 2
    c = img[y0:y0 + ch, x0:x0 + cw]
    return np.repeat(np.repeat(c, k, axis=0), k, axis=1)


def strip(frames, name, labels, cw, ch, k, title):
    cells = []
    counts = []
    for i, lab in enumerate(labels):
        off = graded(os.path.join(frames, "%s_%d_off.ppm.f32" % (name, i)))
        on = graded(os.path.join(frames, "%s_%d_on.ppm.f32" % (name, i)))
        changed = (np.abs(on.astype(int) - off.astype(int)).max(axis=2) >= 3)
        counts.append((lab, int(changed.sum())))
        cells.append((lab, crop(off, cw, ch, k), crop(on, cw, ch, k)))
    cwk, chk = cw * k, ch * k
    gap, top, side = 6, 26, 0
    out = Image.new("RGB", (len(cells) * cwk + (len(cells) - 1) * gap, top + 22 + 2 * chk + gap), (24, 24, 24))
    d = ImageDraw.Draw(out)
    d.text((6, 6), title + "; top: without the mist, below: with it", fill=(225, 225, 225))
    for i, (lab, a, b) in enumerate(cells):
        x = side + i * (cwk + gap)
        d.text((x + 4, top + 4), "+%s s of life after the strike" % lab, fill=(200, 200, 200))
        out.paste(Image.fromarray(a), (x, top + 22))
        out.paste(Image.fromarray(b), (x, top + 22 + chk + gap))
    return out, counts


def main(argv):
    frames, outdir = argv[1], argv[2]
    os.makedirs(outdir, exist_ok=True)
    only = argv[3].split(",") if len(argv) > 3 else None
    for name, labels, cw, ch, k, title in SHOTS:
        if only and name not in only:
            continue
        img, counts = strip(frames, name, labels, cw, ch, k, title)
        p = os.path.join(outdir, "mist_%s.png" % name)
        img.save(p, optimize=True)
        print("%s: %s (%d bytes); pixels the mist changes: %s" % (name, p, os.path.getsize(p),
              ", ".join("+%s s %d" % c for c in counts)))


if __name__ == "__main__":
    main(sys.argv)
