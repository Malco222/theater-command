#!/usr/bin/env python3
"""The kit's picture laid over one of the game's own stills, so an effect is judged on the ground the game really draws.

The preview's ground is one flat colour. The game's sand is rippled, light and dark, and a soft low dust in a tone
near the ground's can show on the one and be lost on the other (build 1764's young gun dust, 20 November). This lays
the effect alone over a still from the game:

  1. Draw the same frame twice with FX_FLOAT=1, on two grounds (FX_GROUND_RAW=161,146,116 and FX_GROUND_RAW=70,70,70,
     FX_GROUND the same in both, so the kit is told the same colour), and the bare ground once on each (no shot).
  2. python3 -B over_game.py out.png game_still.png shot_a.f32 shot_b.f32 bare_a.f32 bare_b.f32 [dx,dy]

From the two grounds it works out, pixel by pixel in linear light, how much the effect covers and what light it adds;
takes the game's still back through the game's curve (game_curve.py) to linear light; lays the effect over it; and
shows the result through the curve again. dx,dy moves the effect (in the 1072x670 picture) to where the game's eye
has it, when the preview's eye is not quite the game's. Things drawn the same on both grounds (the test's own men)
cannot be told from no cover: leave FX_CREW out. Pictures are 1072x670; the still is scaled to that and the result
saved at the still's own size.
"""
import sys

import numpy as np

import game_curve as gc

W, H = 1072, 670


def linear_of(picture):
    """The linear light the game's curve shows as this picture (rows of 0..255 colours): a fixed point, a few rounds."""
    target = gc.decode(picture)
    light = target.copy()
    for _ in range(12):
        got = gc.decode(gc.lands(light))
        light = np.clip(light * np.where(got > 1e-5, target / np.maximum(got, 1e-5), 1.0), 0.0, 64.0)
        light = np.where(target <= 1e-5, 0.0, light)
    return light


def cover_and_light(shot_a, shot_b, bare_a, bare_b):
    """How much of each pixel the effect covers (0..1) and the light it adds, from one frame on two grounds."""
    apart = bare_a - bare_b
    weight = np.abs(apart)
    through = np.where((weight * weight).sum(axis=1) > 1e-8,
                       ((shot_a - shot_b) * np.sign(apart) * weight).sum(axis=1) / np.maximum((weight * weight).sum(axis=1), 1e-8), 1.0)
    cover = np.clip(1.0 - through, 0.0, 1.0)
    untouched = np.abs(shot_a - bare_a).sum(axis=1) < 1e-6
    cover[untouched] = 0.0
    light = np.maximum(shot_a - (1.0 - cover)[:, None] * bare_a, 0.0)
    light[untouched] = 0.0
    return cover, light


def _main(argv):
    if len(argv) < 7:
        print(__doc__)
        return
    from PIL import Image
    out, still = argv[1], argv[2]
    frames = [np.fromfile(p, dtype=np.float32).reshape(H * W, 3).astype(float) for p in argv[3:7]]
    cover, light = cover_and_light(*frames)
    if len(argv) > 7:
        dx, dy = [int(v) for v in argv[7].split(",")]
        cover = np.roll(cover.reshape(H, W), (dy, dx), axis=(0, 1)).reshape(H * W)
        light = np.roll(light.reshape(H, W, 3), (dy, dx), axis=(0, 1)).reshape(H * W, 3)
    game = Image.open(still).convert("RGB")
    ground = linear_of(np.asarray(game.resize((W, H), Image.LANCZOS), float).reshape(H * W, 3))
    shown = np.clip(np.round(gc.lands(light + (1.0 - cover)[:, None] * ground)), 0, 255).astype(np.uint8).reshape(H, W, 3)
    Image.fromarray(shown).resize(game.size, Image.LANCZOS).save(out)
    print(f"{out}: the effect covers {int((cover > 0.02).sum())} pixels, most {cover.max():.2f}")


if __name__ == "__main__":
    _main(sys.argv)
