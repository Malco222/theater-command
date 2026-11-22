#!/usr/bin/env python3
"""The land battle's own grade in numbers, and the fire's spread after it (M_FireSpread).

game_curve.py is the road a colour takes to the screen through the campaign camera's grade. The land battle grades its
picture for its fields instead: most of the colour out of the darks and the middles (0.45 and 0.48 of it), the
brights kept, contrast 1.42, half a stop down, a touch of blue in the gain and a film toe of 0.55. That is lands_land
here, on game_curve's film. Under it a mid-bright orange always lands salmon or peach, which is why the fire's colour
is mended after the grade, on the picture as the screen shows it, by fire_spread: the same sums as the material's
code (Tools/build_content.py, FIRE_SPREAD_HLSL). If the land battle's grade changes, change lands_land's numbers.

  python3 land_grade.py lands 255 150 80 [xOVER]   what the land battle shows for a colour the kit writes
  python3 land_grade.py write 70 58 45             what to write (and how far past white) to land on that
  python3 land_grade.py spread 174 116 89          a screen colour of a marked pixel, after the fire's spread
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import game_curve as gc  # noqa: E402


def lands_land(scene, mids=0.48, darks=0.45, brights=1.0, contrast=1.42, gain=(1.0, 1.0, 1.02), stops=-0.5, toe=0.55):
    """Scene light (linear, as the material makes it) to the screen's 0..255 through the land battle's grade."""
    g = dict(gc.GRADE, exposure=2.0 ** stops, contrast=contrast, toe=toe)
    s = np.atleast_2d(np.asarray(scene, float)) * g["exposure"]
    ap1 = gc._mul(gc.sRGB_2_AP1, s)
    luma = ap1 @ gc.AP1_Y
    safe = np.maximum(luma, 1e-10)[:, None]
    dist = (((ap1 / safe) - 1.0) ** 2).sum(axis=1)
    amount = (1.0 - np.exp2(-4.0 * dist)) * (1.0 - np.exp2(-4.0 * g["expand_gamut"] * luma * luma))
    amount = np.where(luma > 1e-10, amount, 0.0)[:, None]
    ap1 = ap1 + (gc._mul(gc.EXPAND, ap1) - ap1) * amount
    luma = ap1 @ gc.AP1_Y
    w_sh = (1.0 - gc._smooth(0.0, 0.09, luma))[:, None]
    w_hi = gc._smooth(0.5, 1.0, luma)[:, None]

    def corr(w, sat):
        y = (w @ gc.AP1_Y)[:, None]
        w = np.maximum(0.0, y + (w - y) * sat)
        w = np.power(w / 0.18, contrast) * 0.18
        return w * np.asarray(gain, float)

    ap1 = corr(ap1, darks) * w_sh + corr(ap1, mids) * (1.0 - w_sh - w_hi) + corr(ap1, brights) * w_hi
    ap1 = ap1 + (gc._mul(gc.BLUE_AP1, ap1) - ap1) * g["blue_correction"]
    ap1 = gc._film(ap1, g)
    ap1 = ap1 + (gc._mul(gc.BLUE_INV_AP1, ap1) - ap1) * g["blue_correction"]
    out = gc.encode(np.maximum(0.0, gc._mul(gc.AP1_2_sRGB, ap1)))
    return out[0] if np.ndim(scene) == 1 else out


def write_for_land(target, most_over=4.0, **kw):
    """The colour to write (0..255, sRGB bytes) and how far past white, for the land battle to show target."""
    t = np.asarray(target, float)
    x = np.log(np.array([1.0, 0.6, 0.2]))
    for _ in range(400):
        f = lands_land(np.exp(x), **kw) - t
        if np.abs(f).max() < 0.05:
            break
        jac = np.zeros((3, 3))
        for k in range(3):
            d = np.zeros(3)
            d[k] = 1e-3
            jac[:, k] = (lands_land(np.exp(x + d), **kw) - lands_land(np.exp(x - d), **kw)) / 2e-3
        step = np.clip(np.linalg.solve(jac + 1e-6 * np.eye(3), -f), -0.5, 0.5)
        x = np.clip(x + step, np.log(1e-5), np.log(most_over))
    scene = np.exp(x)
    over = max(1.0, float(scene.max()))
    return gc.encode(scene / over), over, lands_land(scene, **kw)


SPREAD = dict(Spread=2.2, Warm=1.2, SatLo=0.30, SatHi=0.45, BrightLo=0.55, BrightHi=0.75, PaleLo=0.08, PaleHi=0.18,
              HueA=-25.0, HueB=-12.0, HueC=55.0, HueD=65.0, LumLo=0.01, LumHi=0.04)


def _ss(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def fire_spread(c, mask=None, p=SPREAD):
    """Screen colours 0..1 (n x 3) of pixels the kit marked (mask 1) after M_FireSpread, and each one's heat."""
    c = np.clip(np.atleast_2d(np.asarray(c, float)), 0, 1)
    mask = np.ones(len(c)) if mask is None else np.asarray(mask, float)
    mx = c.max(1)
    mn = c.min(1)
    s = (mx - mn) / np.maximum(mx, 1e-4)
    h = np.degrees(np.arctan2(np.sqrt(3.0) * (c[:, 1] - c[:, 2]), 2 * c[:, 0] - c[:, 1] - c[:, 2]))
    hue = _ss(p["HueA"], p["HueB"], h) * (1 - _ss(p["HueC"], p["HueD"], h))
    y = c @ np.array([0.2126, 0.7152, 0.0722])
    sat = np.maximum(_ss(p["SatLo"], p["SatHi"], s), _ss(p["BrightLo"], p["BrightHi"], y) * _ss(p["PaleLo"], p["PaleHi"], s))
    heat = mask * hue * sat * _ss(p["LumLo"], p["LumHi"], y)
    o = y[:, None] + (c - y[:, None]) * (1 + (p["Spread"] - 1) * heat)[:, None]
    o[:, 1] *= 1 + (p["Warm"] - 1) * heat
    return np.clip(o, 0, 1), heat


if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) >= 4 and args[0] in ("lands", "write", "spread"):
        rgb = [float(v) for v in args[1:4]]
        if args[0] == "lands":
            over = float(args[4][1:]) if len(args) > 4 and args[4].startswith("x") else 1.0
            print(np.round(lands_land(gc.written(rgb, over, 4.0))).astype(int))
        elif args[0] == "write":
            written, over, shown = write_for_land(rgb)
            print("write", np.round(written).astype(int), "x%.2f" % over, "lands", np.round(shown).astype(int))
        else:
            out, heat = fire_spread(np.array(rgb) / 255.0)
            print(np.round(out[0] * 255).astype(int), "heat %.2f" % heat[0])
    else:
        print(__doc__)
