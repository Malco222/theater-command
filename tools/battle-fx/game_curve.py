#!/usr/bin/env python3
"""The game's own picture, worked out here: what a colour the effects kit hands the game comes out as on the screen.

The kit's colours are unlit light (M_Highlight: vertex colour x Brightness). The game then puts every pixel through
Unreal's film tone curve and the grade set in Source/TheaterCommand/Campaign/CampaignCameraPawn.cpp (a fixed exposure,
saturation 1.07, contrast 1.08, warm highlights, cool shadows, film toe 0.6). That darkens and warms the darks,
deepens strong colours and holds plain white down to about 226. This file is that road in numbers, so that a colour
can be written to LAND on a number the art director gives.

Checked against build 1732's stills (16:30Z, 2026-11-19): white lands 228,227,225 (measured about 224); the flash's
root 250,240,200 at 1.5 times white lands 238,234,222 (measured 235,231,216); its orange 235,140,40 lands 234,155,0
(measured 230,152,4). Within about five of a small bright spot; a thin line (a tracer's core) measures lower than
it lands, since the game's upscaling blends it with its edge. If CampaignCameraPawn.cpp's grade changes, change
GRADE here.

  python3 game_curve.py lands 235 140 40            what the game shows for a colour written as it is
  python3 game_curve.py lands 250 240 200 x1.5      the same, written 1.5 times past white (a light's `over`)
  python3 game_curve.py write 235 140 40            what to write (and how far past white) to land on that
  python3 game_curve.py show out/mg-sand-f002.ppm.f32 1072 670 shown.png
                                                    a preview frame as the game would show it (FX_FLOAT=1 ./fxpreview ...)
  python3 game_curve.py table                       the recipe's picture.to_land: what to write for the screen to show a
                                                    grey of 0, 15, 30 ... 255. The kit reads a colour off it channel by
                                                    channel (earth colours then land within about 3 of the colour asked)
"""
import sys

import numpy as np

GRADE = dict(exposure=1.0, saturation=1.07, contrast=1.08, gain_highlights=(1.03, 1.0, 0.96),
             gain_shadows=(0.96, 0.99, 1.05), toe=0.6, slope=0.88, shoulder=0.26, black_clip=0.0, white_clip=0.04,
             expand_gamut=1.0, blue_correction=0.6)

_M = np.array
sRGB_2_XYZ = _M([[0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041]])
XYZ_2_sRGB = _M([[3.2409699419, -1.5373831776, -0.4986107603], [-0.9692436363, 1.8759675015, 0.0415550574], [0.0556300797, -0.2039769589, 1.0569715142]])
D65_2_D60 = _M([[1.01303, 0.00610531, -0.014971], [0.00769823, 0.998165, -0.00503203], [-0.00284131, 0.00468516, 0.924507]])
D60_2_D65 = _M([[0.987224, -0.00611327, 0.0159533], [-0.00759836, 1.00186, 0.00533002], [0.00307257, -0.00509595, 1.08168]])
XYZ_2_AP1 = _M([[1.6410233797, -0.3248032942, -0.2364246952], [-0.6636628587, 1.6153315917, 0.0167563477], [0.0117218943, -0.0082844420, 0.9883948585]])
AP1_2_XYZ = _M([[0.6624541811, 0.1340042065, 0.1561876870], [0.2722287168, 0.6740817658, 0.0536895174], [-0.0055746495, 0.0040607335, 1.0103391003]])
XYZ_2_AP0 = _M([[1.0498110175, 0.0, -0.0000974845], [-0.4959030231, 1.3733130458, 0.0982400361], [0.0, 0.0, 0.9912520182]])
AP0_2_XYZ = _M([[0.9525523959, 0.0, 0.0000936786], [0.3439664498, 0.7281660966, -0.0721325464], [0.0, 0.0, 1.0088251844]])
AP1_Y = _M([0.2722287168, 0.6740817658, 0.0536895174])
sRGB_2_AP1 = XYZ_2_AP1 @ D65_2_D60 @ sRGB_2_XYZ
AP1_2_sRGB = XYZ_2_sRGB @ D60_2_D65 @ AP1_2_XYZ
AP0_2_AP1 = XYZ_2_AP1 @ AP0_2_XYZ
AP1_2_AP0 = XYZ_2_AP0 @ AP1_2_XYZ
_WIDE_2_XYZ = _M([[0.5441691, 0.2395926, 0.1666943], [0.2394656, 0.7021530, 0.0583814], [-0.0023439, 0.0361834, 1.0552183]])
EXPAND = (XYZ_2_AP1 @ _WIDE_2_XYZ) @ AP1_2_sRGB
_BLUE = _M([[0.9404372683, -0.0183068787, 0.0778696104], [0.0083786969, 0.8286599939, 0.1629613092], [0.0005471261, -0.0008833746, 1.0003362486]])
_BLUE_INV = _M([[1.06318, 0.0233956, -0.0865726], [-0.0106337, 1.20632, -0.19569], [-0.000590887, 0.00105248, 0.999538]])
BLUE_AP1 = AP0_2_AP1 @ _BLUE @ AP1_2_AP0
BLUE_INV_AP1 = AP0_2_AP1 @ _BLUE_INV @ AP1_2_AP0


def _mul(m, c):
    return c @ m.T


def _smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def decode(picture):
    """Picture values 0..255 to linear light."""
    c = np.clip(np.asarray(picture, float) / 255.0, 0.0, 1.0)
    return np.where(c <= 0.04045, c / 12.92, np.power((c + 0.055) / 1.055, 2.4))


def encode(linear):
    """Linear light to picture values 0..255."""
    c = np.clip(np.asarray(linear, float), 0.0, 1.0)
    return 255.0 * np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


def _film(ap1, g):
    """Unreal's film curve on ACEScg colours, an array of rows."""
    ap0 = _mul(AP1_2_AP0, ap1)
    lo, hi = ap0.min(axis=1), ap0.max(axis=1)
    sat = (np.maximum(hi, 1e-10) - np.maximum(lo, 1e-10)) / np.maximum(hi, 1e-2)
    r, gr, b = ap0[:, 0], ap0[:, 1], ap0[:, 2]
    chroma = np.sqrt(np.maximum(b * (b - gr) + gr * (gr - r) + r * (r - b), 0.0))
    yc = (b + gr + r + 1.75 * chroma) / 3.0
    x = (sat - 0.4) / 0.2
    t = np.maximum(1.0 - np.abs(0.5 * x), 0.0)
    shaper = 0.5 * (1.0 + np.sign(x) * (1.0 - t * t))
    gain_in, mid = 0.05 * shaper, 0.08
    glow = np.where(yc <= 2.0 / 3.0 * mid, gain_in, np.where(yc >= 2.0 * mid, 0.0, gain_in * (mid / np.maximum(yc, 1e-10) - 0.5)))
    ap0 = ap0 * (1.0 + glow)[:, None]
    r, gr, b = ap0[:, 0], ap0[:, 1], ap0[:, 2]
    hue = np.degrees(np.arctan2(np.sqrt(3.0) * (gr - b), 2.0 * r - gr - b))
    hue = np.where((r == gr) & (gr == b), 0.0, np.where(hue < 0.0, hue + 360.0, hue))
    centred = np.where(hue > 180.0, hue - 360.0, hue)
    weight = _smooth(0.0, 1.0, 1.0 - np.abs(2.0 * centred / 135.0)) ** 2
    ap0 = ap0.copy()
    ap0[:, 0] = r + weight * sat * (0.03 - r) * (1.0 - 0.82)
    w = np.maximum(0.0, _mul(AP0_2_AP1, ap0))
    y = (w @ AP1_Y)[:, None]
    w = y + (w - y) * 0.96
    toe, slope, shoulder = g["toe"], g["slope"], g["shoulder"]
    toe_scale = 1.0 + g["black_clip"] - toe
    shoulder_scale = 1.0 + g["white_clip"] - shoulder
    in_match = out_match = 0.18
    if toe > 0.8:
        toe_match = (1.0 - toe - out_match) / slope + np.log10(in_match)
    else:
        bt = (out_match + g["black_clip"]) / toe_scale - 1.0
        toe_match = np.log10(in_match) - 0.5 * np.log((1.0 + bt) / (1.0 - bt)) * (toe_scale / slope)
    straight_match = (1.0 - toe) / slope - toe_match
    shoulder_match = shoulder / slope - straight_match
    lg = np.log10(np.maximum(w, 1e-10))
    straight = slope * (lg + straight_match)
    toe_c = -g["black_clip"] + (2.0 * toe_scale) / (1.0 + np.exp((-2.0 * slope / toe_scale) * (lg - toe_match)))
    sh_c = (1.0 + g["white_clip"]) - (2.0 * shoulder_scale) / (1.0 + np.exp((2.0 * slope / shoulder_scale) * (lg - shoulder_match)))
    toe_c = np.where(lg < toe_match, toe_c, straight)
    sh_c = np.where(lg > shoulder_match, sh_c, straight)
    t = np.clip((lg - toe_match) / (shoulder_match - toe_match), 0.0, 1.0)
    if shoulder_match < toe_match:
        t = 1.0 - t
    t = (3.0 - 2.0 * t) * t * t
    tone = toe_c + (sh_c - toe_c) * t
    y = (tone @ AP1_Y)[:, None]
    return np.maximum(0.0, y + (tone - y) * 0.93)


def _correct(w, g, gain):
    y = (w @ AP1_Y)[:, None]
    w = np.maximum(0.0, y + (w - y) * g["saturation"])
    w = np.power(w / 0.18, g["contrast"]) * 0.18
    return w * np.asarray(gain, float)


def lands(scene, grade=None):
    """What the game shows (0..255) for scene light: linear, 1 is the kit's plain white. One colour or rows of them."""
    g = dict(GRADE, **(grade or {}))
    s = np.atleast_2d(np.asarray(scene, float)) * g["exposure"]
    ap1 = _mul(sRGB_2_AP1, s)
    luma = ap1 @ AP1_Y
    safe = np.maximum(luma, 1e-10)[:, None]
    dist = (((ap1 / safe) - 1.0) ** 2).sum(axis=1)
    amount = (1.0 - np.exp2(-4.0 * dist)) * (1.0 - np.exp2(-4.0 * g["expand_gamut"] * luma * luma))
    amount = np.where(luma > 1e-10, amount, 0.0)[:, None]
    ap1 = ap1 + (_mul(EXPAND, ap1) - ap1) * amount
    luma = ap1 @ AP1_Y
    w_sh = (1.0 - _smooth(0.0, 0.09, luma))[:, None]
    w_hi = _smooth(0.5, 1.0, luma)[:, None]
    ap1 = (_correct(ap1, g, g["gain_shadows"]) * w_sh + _correct(ap1, g, (1.0, 1.0, 1.0)) * (1.0 - w_sh - w_hi)
           + _correct(ap1, g, g["gain_highlights"]) * w_hi)
    ap1 = ap1 + (_mul(BLUE_AP1, ap1) - ap1) * g["blue_correction"]
    ap1 = _film(ap1, g)
    ap1 = ap1 + (_mul(BLUE_INV_AP1, ap1) - ap1) * g["blue_correction"]
    out = encode(np.maximum(0.0, _mul(AP1_2_sRGB, ap1)))
    return out[0] if np.ndim(scene) == 1 else out


def written(picture, over=1.0, lit_gain=4.0):
    """The scene light the game gets for a recipe colour: linear, cut to 8 bits after dividing by picture.lit_gain."""
    v = decode(picture) * over / lit_gain
    return np.round(np.clip(v, 0.0, 1.0) * 255.0) / 255.0 * lit_gain


def write_for(target, most_over=4.0):
    """The recipe colour and `over` whose picture in the game is target. Returns (colour 0..255, over, what it lands)."""
    t = np.asarray(target, float)
    x = np.log(np.array([1.0, 0.6, 0.2]))
    for _ in range(300):
        f = lands(np.exp(x)) - t
        if np.abs(f).max() < 0.02:
            break
        jac = np.zeros((3, 3))
        for k in range(3):
            d = np.zeros(3)
            d[k] = 1e-3
            jac[:, k] = (lands(np.exp(x + d)) - lands(np.exp(x - d))) / 2e-3
        step = np.clip(np.linalg.solve(jac + 1e-6 * np.eye(3), -f), -0.5, 0.5)
        x = np.clip(x + step, np.log(1e-5), np.log(most_over))
    scene = np.exp(x)
    over = max(1.0, float(scene.max()))
    return encode(scene / over), over, lands(scene)


def show(path, width, height, out):
    """A preview frame's linear light (FX_FLOAT=1) as the game would show it."""
    from PIL import Image
    raw = np.fromfile(path, dtype=np.float32).reshape(height * width, 3).astype(float)
    Image.fromarray(np.round(lands(raw)).astype(np.uint8).reshape(height, width, 3)).save(out)


def _main(argv):
    if len(argv) >= 5 and argv[1] == "lands":
        over = float(argv[5][1:]) if len(argv) > 5 and argv[5].startswith("x") else 1.0
        colour = [float(v) for v in argv[2:5]]
        print("lands", np.round(lands(written(colour, over, max(4.0, over))), 1))
    elif len(argv) >= 5 and argv[1] == "write":
        colour, over, got = write_for([float(v) for v in argv[2:5]])
        print("write", np.round(colour), "over", round(over, 2), "lands", np.round(got, 1))
    elif len(argv) >= 2 and argv[1] == "table":
        print([round(float(np.mean(write_for([v, v, v])[0])), 1) for v in range(0, 256, 15)])
    elif len(argv) >= 6 and argv[1] == "show":
        show(argv[2], int(argv[3]), int(argv[4]), argv[5])
    else:
        print(__doc__)


if __name__ == "__main__":
    _main(sys.argv)
