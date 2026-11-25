#!/usr/bin/env python3
"""A cloud's own colour, with nothing of the sky or the ground showing through it.

Draw the same frame twice with the preview's flat backdrop, once on black and once on white:

  FX_FLOAT=1 FX_BACK=0,0,0       ./fxpreview rec.txt on_black shell how105 earth 60 480x360 100 6
  FX_FLOAT=1 FX_BACK=255,255,255 ./fxpreview rec.txt on_white shell how105 earth 60 480x360 100 6
  python3 own_colour.py on_black/f.ppm.f32 on_white/f.ppm.f32 480 360 [own.png]

The two differ only by what shows through, so every pixel's cover is 1 - (white - black), and where the cover is
whole the picture on black IS the cloud's own colour. It prints, as the game shows them (game_curve.py), on the
art director's scale (the brightest channel, 0 to 255, and the same of 100):

  the lit top      the 90th and 97th percentile of the well-covered pixels in the top third of the cloud's height
  the shade        the 10th percentile there
  the darkest      the mean of the darkest quarter of all the well-covered pixels, and where that quarter ends
  the warm share   the part of them clearly redder than blue (lit by the flash, or the flash itself)

own.png: the cloud's own colour over mid grey, to look at.
"""
import sys

import numpy as np

import game_curve


def read(path, width, height):
    return np.fromfile(path, dtype=np.float32).reshape(height, width, 3).astype(float)


def own(on_black, on_white, width, height, cover_least=0.9):
    k, w = read(on_black, width, height), read(on_white, width, height)
    cover = np.clip(1.0 - (w - k).mean(axis=2), 0.0, 1.0)  # (white is 1.0 of linear light, black 0)
    shown = game_curve.lands(k.reshape(-1, 3)).reshape(height, width, 3)
    return cover, shown, cover >= cover_least


def measure(cover, shown, solid):
    if solid.sum() < 50:
        return None
    value = shown.max(axis=2)
    rows = np.nonzero(solid.any(axis=1))[0]
    top_row, foot_row = rows.min(), rows.max()
    in_top = solid & (np.arange(shown.shape[0])[:, None] < top_row + (foot_row - top_row) / 3.0)
    every = np.sort(value[solid])
    quarter = every[:max(1, len(every) // 4)]
    warm = solid & (shown[..., 0] > 1.25 * shown[..., 2] + 20.0)
    return dict(pixels=int(solid.sum()), lit90=float(np.percentile(value[in_top], 90)), lit97=float(np.percentile(value[in_top], 97)),
                shade10=float(np.percentile(value[in_top], 10)), dark=float(quarter.mean()), dark_ends=float(quarter[-1]),
                median=float(every[len(every) // 2]), warm=float(warm.sum() / solid.sum()),
                dark_colour=[float(v) for v in shown[solid][np.argsort(value[solid])[:max(1, len(every) // 4)]].mean(axis=0)])


def _main(argv):
    if len(argv) < 5:
        print(__doc__)
        return
    width, height = int(argv[3]), int(argv[4])
    cover, shown, solid = own(argv[1], argv[2], width, height)
    m = measure(cover, shown, solid)
    if m is None:
        print("nothing well covered")
        return
    print("well covered %d px: lit top 90th %.0f, 97th %.0f (%.0f, %.0f of 100); shade 10th %.0f (%.0f); darkest quarter mean %.0f (%.0f of 100) "
          "colour %.0f,%.0f,%.0f, ends at %.0f; median %.0f; warm share %.2f"
          % (m["pixels"], m["lit90"], m["lit97"], m["lit90"] / 2.55, m["lit97"] / 2.55, m["shade10"], m["shade10"] / 2.55, m["dark"],
             m["dark"] / 2.55, *m["dark_colour"], m["dark_ends"], m["median"], m["warm"]))
    if len(argv) > 5:
        from PIL import Image
        grey = np.full_like(shown, 128.0)
        a = cover[..., None]
        Image.fromarray(np.round(np.clip(shown + grey * (1.0 - a), 0.0, 255.0)).astype(np.uint8)).save(argv[5])


if __name__ == "__main__":
    _main(sys.argv)
