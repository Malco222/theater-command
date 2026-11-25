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
  python3 land_grade.py --grade desert write ...   the same under another ground's grade (GRADES)
  python3 land_grade.py retarget fx.json desert [--apply] [--no-spread]
                                                   rewrite every land_grade.<path> listed in land_grade._targets
                                                   (the landed colours the art director passed) for that grade, and
                                                   (unless --no-spread) for the fire's spread after it, which takes
                                                   a sand-coloured kit pixel for fire

A grade per recording (the art director's ruling of 17:52Z 22 November, art-director/battles/theatres-sheet-rulings-
1122.md): each ground's recording carries its own sky.grade. GRADES holds Normandy's as passed and the starting
numbers for the others; change them here as the recordings are tuned. What the art director passes is the landed
colour, so land_grade._targets keeps it, and retarget writes the colours again when a ground's grade changes.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import game_curve as gc  # noqa: E402


GRADES = {
    "normandy": dict(mids=0.48, darks=0.45, contrast=1.42, stops=-0.5, toe=0.55, gain=(1.0, 1.0, 1.02)),
    "desert": dict(mids=0.66, darks=0.55, contrast=1.25, stops=-0.6, toe=0.55, gain=(1.02, 1.0, 0.96)),
    "med": dict(mids=0.58, darks=0.50, contrast=1.35, stops=-0.5, toe=0.55, gain=(1.02, 1.0, 0.97)),
    "jungle": dict(mids=0.66, darks=0.60, contrast=1.15, stops=-0.2, toe=0.45, gain=(1.0, 1.0, 1.0)),
    "steppe": dict(mids=0.55, darks=0.50, contrast=1.35, stops=-0.5, toe=0.55, gain=(1.01, 1.0, 0.99)),
    "winter": dict(mids=0.50, darks=0.45, contrast=1.30, stops=-0.7, toe=0.55, gain=(0.99, 1.0, 1.03)),
}


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


SPREAD = dict(Spread=2.0, Warm=1.2, SatLo=0.30, SatHi=0.45, BrightLo=0.70, BrightHi=0.85, PaleLo=0.08, PaleHi=0.18,
              TopLo=0.85, TopHi=0.95, LitLo=0.66, LitHi=0.78, LitSatLo=0.12, LitSatHi=0.20,
              LitHueA=95.0, LitHueB=108.0, LitHueC=160.0, LitHueD=175.0,
              HueA=-25.0, HueB=-12.0, HueC=55.0, HueD=65.0, WarmHueLo=32.0, WarmHueHi=42.0, LumLo=0.01, LumHi=0.04)


def _ss(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def fire_spread(c, mask=None, p=SPREAD):
    """Screen colours 0..1 (n x 3) of pixels the kit marked (mask 1) after M_FireSpread, and each one's heat.

    fire: orange to yellow (hue), deep in colour or bright and a little coloured. lit: a bright, coloured pixel of
    green hue (a Soviet tracer), which the grade leaves grey-green; red is fire's hue already, and blue and violet are
    the sky's, left alone. Neither touches the whitest (top: a white-hot core stays as ruled). Warm raises green only
    on fire's orange (on yellow it went lime, on the sky cyan)."""
    c = np.clip(np.atleast_2d(np.asarray(c, float)), 0, 1)
    mask = np.ones(len(c)) if mask is None else np.asarray(mask, float)
    mx = c.max(1)
    mn = c.min(1)
    s = (mx - mn) / np.maximum(mx, 1e-4)
    hx = 2 * c[:, 0] - c[:, 1] - c[:, 2]
    hy = np.sqrt(3.0) * (c[:, 1] - c[:, 2])
    h = np.where(np.abs(hx) + np.abs(hy) > 1e-5, np.degrees(np.arctan2(hy, hx)), 0.0)
    hue = _ss(p["HueA"], p["HueB"], h) * (1 - _ss(p["HueC"], p["HueD"], h))
    y = c @ np.array([0.2126, 0.7152, 0.0722])
    top = 1 - _ss(p["TopLo"], p["TopHi"], y)
    fire = hue * np.maximum(_ss(p["SatLo"], p["SatHi"], s), _ss(p["BrightLo"], p["BrightHi"], y) * _ss(p["PaleLo"], p["PaleHi"], s) * top)
    green = _ss(p["LitHueA"], p["LitHueB"], h) * (1 - _ss(p["LitHueC"], p["LitHueD"], h))
    lit = green * _ss(p["LitLo"], p["LitHi"], y) * _ss(p["LitSatLo"], p["LitSatHi"], s) * top
    heat = mask * np.maximum(fire, lit) * _ss(p["LumLo"], p["LumHi"], y)
    o = y[:, None] + (c - y[:, None]) * (1 + (p["Spread"] - 1) * heat)[:, None]
    o[:, 1] *= 1 + (p["Warm"] - 1) * mask * fire * _ss(p["LumLo"], p["LumHi"], y) * (1 - _ss(p["WarmHueLo"], p["WarmHueHi"], h))
    return np.clip(o, 0, 1), heat


def unspread(target, p=SPREAD):
    """The screen colour (0..255) a kit pixel must have after the grade for M_FireSpread to leave it at target.

    The spread takes any marked pixel of fire's hue (orange to yellow) that is deep in colour, or bright and a little
    coloured, for fire, and spreads its colour: a sand-coloured dust or burst is of that hue, so one written to land on
    its colour after the grade alone lands deeper (the desert's tank dust went mustard, 201,158,50: art director,
    03:04Z 24 November). Of the colours that spread to target, the one nearest it."""
    t = np.asarray(target, float) / 255.0
    y = t @ np.array([0.2126, 0.7152, 0.0722])

    def sat(v):
        return (v.max() - v.min()) / max(v.max(), 1e-6)

    def polish(c):
        for _ in range(300):
            f = fire_spread(c, p=p)[0][0] - t
            if np.abs(f).max() < 1e-5:
                break
            jac = np.zeros((3, 3))
            for k in range(3):
                d = np.zeros(3)
                d[k] = 1e-4
                jac[:, k] = (fire_spread(c + d, p=p)[0][0] - fire_spread(c - d, p=p)[0][0]) / 2e-4
            c = np.clip(c + 0.5 * np.linalg.solve(jac + 1e-9 * np.eye(3), -f), 0.0, 1.0)
        return c, np.abs(fire_spread(c, p=p)[0][0] - t).max()

    lo, hi = 0.2, 1.0
    for _ in range(60):
        k = 0.5 * (lo + hi)
        if sat(fire_spread(y + (t - y) * k, p=p)[0][0]) > sat(t):
            hi = k
        else:
            lo = k
    tries = [polish(t.copy()), polish(np.clip(y + (t - y) * 0.5 * (lo + hi), 0.0, 1.0))]
    good = [c for c, miss in tries if miss < 0.5 / 255.0] or [min(tries, key=lambda cm: cm[1])[0]]
    return min(good, key=lambda c: np.abs(c - t).max()) * 255.0


def retarget(path, grade, apply=False, spread=True):
    """Every land_grade._targets entry written again for grade; with apply, into the recipe at path. With spread (as
    the land battle draws, its lens carrying M_FireSpread since 22 November), each lands on its target after the fire's
    spread too."""
    import json
    from collections import OrderedDict
    with open(path) as f:
        book = json.load(f, object_pairs_hook=OrderedDict)
    row = book["land_grade"]
    kw = GRADES[grade]

    def walk(node, at):
        for key, val in node.items():
            if key.startswith("_"):
                continue
            if isinstance(val, dict):
                yield from walk(val, at + [key])
            else:
                yield at + [key], val

    for keys, target in walk(row.get("_targets", {}), []):
        graded = unspread(target) if spread else np.asarray(target, float)
        written, over, shown = write_for_land(graded, **kw)
        put = [int(round(v)) for v in written]
        seen = lands_land(gc.written(put, 1.0, 4.0), **kw)
        if spread:
            seen = fire_spread(np.asarray(seen) / 255.0)[0][0] * 255.0
        print(".".join(keys), "target", list(target), "write", put, "x%.2f" % over, "lands", np.round(seen).astype(int).tolist())
        node = row
        for key in keys[:-1]:
            node = node.setdefault(key, OrderedDict())
        node[keys[-1]] = put
    row["_targets_grade"] = grade + (" with the fire's spread" if spread else "")
    if apply:
        with open(path, "w") as f:
            f.write(json.dumps(book, indent=1, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    args = sys.argv[1:]
    grade = "normandy"
    if len(args) >= 2 and args[0] == "--grade":
        grade, args = args[1], args[2:]
    kw = GRADES[grade]
    if len(args) >= 3 and args[0] == "retarget":
        retarget(args[1], args[2], "--apply" in args, "--no-spread" not in args)
    elif len(args) >= 4 and args[0] in ("lands", "write", "spread"):
        rgb = [float(v) for v in args[1:4]]
        if args[0] == "lands":
            over = float(args[4][1:]) if len(args) > 4 and args[4].startswith("x") else 1.0
            print(np.round(lands_land(gc.written(rgb, over, 4.0), **kw)).astype(int))
        elif args[0] == "write":
            written, over, shown = write_for_land(rgb, **kw)
            print("write", np.round(written).astype(int), "x%.2f" % over, "lands", np.round(shown).astype(int))
        else:
            out, heat = fire_spread(np.array(rgb) / 255.0)
            print(np.round(out[0] * 255).astype(int), "heat %.2f" % heat[0])
    else:
        print(__doc__)
