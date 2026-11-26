"""
Paints the ground battle's leaves: the sprays of leaves that M_LandLeaves (Tools/build_content.py) lays on the cards
of the hedge shrubs and the trees (LandField's BuildShrub and BuildTree), so that close by a hedge is leaves and twigs
with the light through its gaps, not a heap of green gems (Brian, 22 November: "i want the grass and bushes and plants
to look more realistic"). Everything here is made from numbers with a fixed seed: nothing is fetched and nothing copied.

    python3 Tools/land_look/make_leaf_textures.py        (numpy and Pillow; a few seconds)

writes, beside this script:

T_LandLeaves.png   1024 px, four sprays in its quarters, each a card's whole picture (its twig's foot at the bottom
                   middle, the leaves spreading up and out from it):
                       top left      hawthorn and blackthorn: many small leaves, lobed, close set
                       top right     hazel and field maple: round leaves, larger, fewer
                       bottom left   oak: lobed leaves in bunches at the twigs' ends
                       bottom right  elm, beech, apple: oval leaves, pointed
                   RGB  linear numbers, not colours: 0.5 is the card's paint as it is (the vertex colour's leaf green);
                        each leaf a little lighter or darker, warmer or cooler, paler at its lit tip, a pale midrib,
                        darker toward its stalk; the twigs dark (a third of the paint).
                   A    the leaves and twigs (1); between them (0, written as 1/255: Unreal's PNG import fills the
                        colour of fully clear pixels, and the colour between the leaves is each nearest leaf's, so the
                        smaller pictures the engine makes from it keep the leaves' colour at their edges).

The cloud's painter (Tools/land_field/land_field_picture.cpp) reads the same picture, so the pictures it paints show
the cards as the game draws them.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
N = 1024
CELL = N // 2
SS = 4  # drawn at four times the size, then brought down: soft edges
SEED = 19440613  # Villers-Bocage


def leaf_outline(kind, length, width, rng):
    """A leaf's outline as points along its midrib's frame: x along the leaf from its stalk (0) to its tip (length),
    y across it. kind: 'thorn' (three lobes, small), 'round' (hazel: round, a short point), 'oak' (deep round lobes),
    'oval' (elm, beech, apple: an oval to a point)."""
    pts_up = []
    steps = 18
    for i in range(steps + 1):
        t = i / steps
        if kind == "oval":
            w = math.sin(math.pi * t ** 0.85) * (1.0 - 0.25 * t)
        elif kind == "round":
            w = math.sin(math.pi * t ** 0.7) ** 0.8
        elif kind == "thorn":
            w = math.sin(math.pi * t ** 0.8) * (0.75 + 0.35 * math.sin(3 * math.pi * t) ** 2)
        else:  # oak
            w = (0.55 + 0.45 * abs(math.sin(4.5 * math.pi * t))) * math.sin(math.pi * t ** 0.9)
        w *= 0.5 * width * (1.0 + 0.06 * rng.standard_normal())
        pts_up.append((t * length, w))
    pts_down = [(x, -y * (1.0 + 0.05 * rng.standard_normal())) for x, y in reversed(pts_up)]
    return pts_up + pts_down


def place(points, ox, oy, angle, scale=1.0):
    c, s = math.cos(angle), math.sin(angle)
    return [(ox + scale * (x * c - y * s), oy + scale * (x * s + y * c)) for x, y in points]


def spray(kind, rng, size):
    """One card's spray: twigs from the foot, leaves along them. Returns (value, warm, alpha) arrays, size x size, each
    drawn at SS times and brought down."""
    S = size * SS
    alpha = Image.new("L", (S, S), 0)
    value = Image.new("L", (S, S), 0)   # 128 is 0.5 (as painted)
    warm = Image.new("L", (S, S), 128)  # 128 neither
    da, dv, dw = ImageDraw.Draw(alpha), ImageDraw.Draw(value), ImageDraw.Draw(warm)

    if kind == "thorn":
        twigs, leaves_per, leaf_len, leaf_w, spread = 8, 18, 0.075, 0.055, 0.85
    elif kind == "round":
        twigs, leaves_per, leaf_len, leaf_w, spread = 6, 8, 0.12, 0.105, 0.9
    elif kind == "oak":
        twigs, leaves_per, leaf_len, leaf_w, spread = 7, 11, 0.13, 0.075, 0.9
    else:
        twigs, leaves_per, leaf_len, leaf_w, spread = 7, 13, 0.12, 0.065, 0.9

    foot = (0.5 * S, 0.985 * S)
    leaves = []  # (outline in pixels, tone, warm, depth order)
    twig_lines = []
    for k in range(twigs):
        # each twig fans out from the foot, curving, its end within the card
        a0 = -math.pi / 2 + spread * (k - (twigs - 1) / 2) / max(1, (twigs - 1) / 2) * 0.95 + 0.12 * rng.standard_normal()
        length = S * (0.62 + 0.3 * rng.random()) * (1.0 - 0.25 * abs(math.sin(a0 + math.pi / 2)))
        bend = 0.5 * rng.standard_normal()
        pts = []
        x, y = foot
        n = 24
        for i in range(n + 1):
            t = i / n
            a = a0 + bend * t * t
            pts.append((x, y))
            x += math.cos(a) * length / n
            y += math.sin(a) * length / n
            x = min(max(x, 0.04 * S), 0.96 * S)
            y = min(max(y, 0.04 * S), 0.99 * S)
        twig_lines.append(pts)
        for j in range(leaves_per):
            t = 0.22 + 0.78 * (j + rng.random() * 0.6) / leaves_per
            if kind == "oak" and t < 0.6 and rng.random() < 0.6:
                continue  # oak leaves bunch at the twigs' ends
            i = min(n, int(t * n))
            px, py = pts[i]
            qx, qy = pts[min(n, i + 1)] if i < n else pts[i - 1]
            along = math.atan2(qy - py, qx - px) if i < n else math.atan2(py - qy, px - qx)
            side = 1 if j % 2 == 0 else -1
            ang = along + side * (0.75 + 0.45 * rng.random())
            L = leaf_len * S * (0.8 + 0.4 * rng.random()) * (1.0 - 0.25 * t if kind != "oak" else 1.0)
            W = leaf_w * S * (0.85 + 0.3 * rng.random())
            outline = place(leaf_outline(kind if kind != "round" else "round", 1.0, W / L, rng), px, py, ang, L)
            tone = 0.47 * (1.0 + 0.16 * rng.standard_normal()) * (0.9 + 0.2 * t)
            tint = 0.5 + 0.08 * rng.standard_normal()
            leaves.append((outline, (px, py, ang, L), tone, tint, rng.random()))
    # a few leaves straight on the foot's stem, filling the card's middle
    for j in range(int(leaves_per * 0.8)):
        px = foot[0] + S * 0.28 * rng.standard_normal()
        py = foot[1] - S * (0.2 + 0.6 * rng.random())
        px = min(max(px, 0.1 * S), 0.9 * S)
        ang = -math.pi / 2 + 1.1 * rng.standard_normal()
        L = leaf_len * S * (0.8 + 0.4 * rng.random())
        W = leaf_w * S * (0.85 + 0.3 * rng.random())
        outline = place(leaf_outline(kind, 1.0, W / L, rng), px, py, ang, L)
        leaves.append((outline, (px, py, ang, L), 0.5 * (1.0 + 0.16 * rng.standard_normal()), 0.5 + 0.08 * rng.standard_normal(), rng.random()))

    # twigs first (under the leaves), dark
    for pts in twig_lines:
        for i in range(len(pts) - 1):
            w = max(1, int(SS * (2.2 - 1.6 * i / len(pts)) * size / 512))
            da.line([pts[i], pts[i + 1]], fill=255, width=w)
            dv.line([pts[i], pts[i + 1]], fill=int(128 * 0.36), width=w)
    # the leaves, back to front by their own order: each body, then a darker stalk end and a pale midrib
    leaves.sort(key=lambda L: L[4])
    for outline, (px, py, ang, L), tone, tint, _ in leaves:
        da.polygon(outline, fill=255)
        v = int(np.clip(tone, 0.2, 0.9) * 256)
        dv.polygon(outline, fill=min(255, v))
        dw.polygon(outline, fill=int(np.clip(tint, 0.2, 0.8) * 255))
        # lit toward its tip and its middle, shaded at its stalk: a smaller copy of the leaf, paler, set toward the tip
        inner = [(px + (x - px) * 0.62 + math.cos(ang) * L * 0.3, py + (y - py) * 0.62 + math.sin(ang) * L * 0.3) for x, y in outline]
        dv.polygon(inner, fill=min(255, int(v * 1.1)))
        mid = [(px, py), (px + math.cos(ang) * L * 0.9, py + math.sin(ang) * L * 0.9)]
        dv.line(mid, fill=min(255, int(v * 1.18)), width=max(1, int(SS * 0.9 * size / 512)))
    # (the bands drawn as discs spill past the leaf: keep them in it)
    small = lambda im: np.asarray(im.resize((size, size), Image.LANCZOS), dtype=np.float64) / 255.0
    a = small(alpha)
    vmask = np.asarray(alpha, dtype=np.float64) / 255.0
    v_full = np.asarray(value, dtype=np.float64) * (vmask > 0.5) + 128.0 * (vmask <= 0.5)
    v = np.asarray(Image.fromarray(np.uint8(np.clip(v_full, 0, 255))).resize((size, size), Image.LANCZOS), dtype=np.float64) / 256.0
    w = small(warm)
    return v, w, a


def dilate_into_clear(rgb, alpha, steps=24):
    """The colour between the leaves: each clear pixel takes its nearest leaf's (grown out a pixel at a time)."""
    have = alpha > 0.5
    out = rgb.copy()
    for _ in range(steps):
        if have.all():
            break
        acc = np.zeros_like(out)
        cnt = np.zeros(have.shape)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            sh = np.roll(np.roll(have, dy, 0), dx, 1)
            acc += np.roll(np.roll(out, dy, 0), dx, 1) * sh[..., None]
            cnt += sh
        grow = (~have) & (cnt > 0)
        out[grow] = acc[grow] / cnt[grow][:, None]
        have = have | grow
    out[~have] = 0.5
    return out


def main():
    rng = np.random.default_rng(SEED)
    img = np.zeros((N, N, 4))
    for (cy, cx), kind in (((0, 0), "thorn"), ((0, 1), "round"), ((1, 0), "oak"), ((1, 1), "oval")):
        v, w, a = spray(kind, rng, CELL)
        warm = (w - 0.5) * 2.0
        rgb = np.stack([v * (1.0 + 0.12 * warm), v, v * (1.0 - 0.2 * warm)], axis=-1)
        rgb = dilate_into_clear(rgb, a)
        img[cy * CELL:(cy + 1) * CELL, cx * CELL:(cx + 1) * CELL, :3] = rgb
        img[cy * CELL:(cy + 1) * CELL, cx * CELL:(cx + 1) * CELL, 3] = a
        cover = float((a > 0.5).mean())
        print(f"  {kind:6s} cell: {cover * 100:.0f}% leaf, value {float(v[a > 0.5].mean()):.3f} on the leaves")
    out = np.uint8(np.clip(np.round(img * 255.0), 0, 255))
    out[..., 3] = np.maximum(out[..., 3], 1)
    path = os.path.join(HERE, "T_LandLeaves.png")
    Image.fromarray(out, "RGBA").save(path, optimize=True)
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
