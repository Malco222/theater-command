"""
Paints the ground battle's own detail pictures, which M_LandSurface (Tools/build_content.py) lays over the field's
vertex colours. Everything here is made from numbers with a fixed seed: nothing is fetched and nothing copied.

    python3 Tools/land_look/make_ground_textures.py        (numpy and Pillow; a few seconds)

writes, beside this script:

T_LandGrain.png    1024 px, about 2 m of ground a repeat. Linear numbers, not colours: 0.5 is "as painted".
                   R  a grazed sward seen from above: blades in four layers, the deep ones in shade, tips lit
                   G  bare soil: clods and crumbs, their partings, grit, flints, a few bits of straw
                   B  the sward's hue: + yellower (a dead blade, a lit tip), - bluer (clover, the green in shade)
                   (The soil's hue follows its brightness in the material: a dry clod's top is paler and warmer,
                   a damp parting darker and colder.)
T_LandBump.png     1024 px, the same 2 m: which way the surface leans, from the same heights.
                   R, G  the sward's lean along u and v; B, A  the soil's. 0.5 is level; 0..1 is -1..1.
T_LandPatch.png    1024 px, about 40 m a repeat: the broad lie of a field.
                   R  value (0.5 as painted)   G  warmth: + dry and yellow, - lush and blue-green
                   B  wear: where the grass is trodden or grazed to the soil (0 none)
                   A  tussocks: the rank, darker, bluer clumps the cattle leave (0 none, 1 the thickest), gathered
                      in some places and missing in others. The material lays them on grass only, never on soil.

Every picture repeats without a seam: every noise is made periodic in the frequency domain, and every blade or clod
that crosses an edge is drawn again on the far side. Alpha is never 0, because Unreal's PNG import fills the colour of
fully clear pixels.
"""
import os
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
N = 1024
SEED = 19440718  # Saint-Lo falls


def periodic_noise(rng, n, low, high, power=1.0):
    """Noise that repeats over n pixels, holding wavelengths between n/high and n/low pixels, with an amplitude
    falling as 1/f**power. Normalised to mean 0, standard deviation 1."""
    white = rng.standard_normal((n, n))
    f = np.fft.fftfreq(n) * n
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx * fx + fy * fy)
    band = np.where((r >= low) & (r <= high), 1.0 / np.maximum(r, 1.0) ** power, 0.0)
    # (soft edges on the band, so no ringing)
    band *= np.clip((r - low + 1.0), 0, 1) * np.clip((high - r + 1.0), 0, 1)
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * band))
    out -= out.mean()
    return out / (out.std() + 1e-9)


def periodic_blur(img, sigma):
    """A Gaussian blur that wraps at the edges."""
    n = img.shape[0]
    f = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(f, f)
    kernel = np.exp(-2.0 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * kernel))


def lean_of(height, strength):
    """Which way a height field leans, in its own pixels: (-dh/du, -dh/dv) times strength, wrapping at the edges."""
    du = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dv = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    return np.clip(-du * strength, -1, 1), np.clip(-dv * strength, -1, 1)


def centred(x, spread, mean=0.5):
    """Numbers moved to a mean and a spread (standard deviation), kept within 0..1."""
    x = (x - x.mean()) / (x.std() + 1e-9)
    return np.clip(mean + x * spread, 0.0, 1.0)


def byte(x, floor=0):
    return np.clip(np.round(x * 255.0), floor, 255).astype(np.uint8)


def coarse(x, bits, floor=0):
    """Numbers kept to fewer steps (it keeps the files small), rounded so that 128, the middle, is one of them."""
    step = 1 << (8 - bits)
    return np.clip(np.round(x * 255.0 / step) * step, floor, 256 - step).astype(np.uint8)


def draw_wrapped(draw, points, fill, width, n, margin):
    """Draws a polyline and its copies across whichever edges it comes near, so the picture repeats."""
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    offs_x = [0]
    offs_y = [0]
    if min(xs) < margin:
        offs_x.append(n)
    if max(xs) > n - margin:
        offs_x.append(-n)
    if min(ys) < margin:
        offs_y.append(n)
    if max(ys) > n - margin:
        offs_y.append(-n)
    for ox in offs_x:
        for oy in offs_y:
            draw.line([(x + ox, y + oy) for x, y in points], fill=fill, width=width)


def ellipse_wrapped(draw, cx, cy, rx, ry, fill, n):
    for ox in (0, n, -n):
        for oy in (0, n, -n):
            x, y = cx + ox, cy + oy
            if -rx <= x <= n + rx and -ry <= y <= n + ry:
                draw.ellipse([x - rx, y - ry, x + rx, y + ry], fill=fill)


def code(i):
    return (i & 255, (i >> 8) & 255, (i >> 16) & 255)


def sward(rng):
    """A grazed July pasture seen from straight above, 2 mm a pixel: blades in four layers, the lower ones in the
    shade of the upper, each blade drawn in four pieces from its foot (low, dark) to its tip (high, lit), some dead
    and straw-yellow, a little clover; between them, the shade at the grass's foot."""
    canvas = Image.new("RGB", (N, N), (0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    # Per piece: height, brightness, hue. Index 0 is the shade between blades.
    height = [0.0]
    bright = [0.0]
    hue = [0.0]
    layers = 4
    # (a broad lie in the sward: denser and taller in places, so it is not an even carpet)
    lush = periodic_noise(rng, N, 3, 24, 1.0)
    for layer in range(layers):
        count = [13000, 12000, 10000, 7000][layer]
        for _ in range(count):
            x, y = rng.uniform(0, N, 2)
            here = lush[int(y) % N, int(x) % N]
            if layer == layers - 1 and rng.random() > 0.55 + 0.25 * here:
                continue
            # Clover: three small round leaves, a little bluer and darker, low in the sward.
            if layer == 1 and rng.random() < 0.035:
                leaf_b = 0.95 + rng.normal(0, 0.06)
                for k in range(3):
                    a = rng.uniform(0, 2 * np.pi) if k == 0 else a + 2.1 + rng.normal(0, 0.2)
                    r = rng.uniform(3.0, 4.5)
                    i = len(height)
                    height.append(0.45 + rng.uniform(0, 0.1))
                    bright.append(leaf_b + rng.normal(0, 0.03))
                    hue.append(-0.3 + rng.normal(0, 0.08))
                    ellipse_wrapped(draw, x + np.cos(a) * r, y + np.sin(a) * r, r, r * 0.9, code(i), N)
                continue
            dead = rng.random() < (0.05 + 0.04 * layer)
            length = rng.uniform(10, 26) * (1.0 + 0.25 * here) * (0.8 + 0.12 * layer)
            angle = rng.uniform(0, 2 * np.pi)
            bend = rng.normal(0, 0.35)
            width = 2 if rng.random() < 0.7 else 3
            blade_b = rng.normal(0, 0.09) + (0.15 if dead else 0.0)
            blade_h = rng.normal(0, 0.12) + (0.75 if dead else 0.0)
            pieces = 4
            px, py = x, y
            for p in range(pieces):
                t0 = p / pieces
                t1 = (p + 1) / pieces
                a = angle + bend * t1
                nx = px + np.cos(a) * length / pieces
                ny = py + np.sin(a) * length / pieces
                i = len(height)
                # (a blade rises from its foot to its tip: the tip is higher and nearer the sun)
                height.append(layer / layers * 0.8 + 0.25 * t1)
                # (the deep layers are in their neighbours' shade; the tip of a blade catches the light)
                bright.append(0.55 + 0.17 * layer + 0.18 * t1 + blade_b + 0.08 * t0 * rng.random())
                hue.append(blade_h + 0.25 * t1 - 0.25 * (layers - 1 - layer) / layers)
                draw_wrapped(draw, [(px, py), (nx, ny)], code(i), width, N, 40)
                px, py = nx, ny
    ids = np.asarray(canvas).astype(np.int64)
    ids = ids[..., 0] + (ids[..., 1] << 8) + (ids[..., 2] << 16)
    height = np.asarray(height)[ids]
    bright = np.asarray(bright)[ids]
    hue = np.asarray(hue)[ids]
    # The gaps: the foot of the sward, dark, a little earth and dead stuff showing.
    gap = ids == 0
    floor = periodic_noise(rng, N, 40, 300, 0.5)
    bright[gap] = 0.22 + 0.06 * floor[gap]
    hue[gap] = 0.35 + 0.2 * floor[gap]
    height[gap] = -0.15
    # (and soft shade under the top layer: what is near a tall blade is darker)
    shade = periodic_blur(height, 3.0)
    bright = bright - 0.35 * np.clip(shade - height, 0, 1)
    return height, bright, hue, gap.mean()


def cells(rng, size, warp):
    """Periodic cells, one jittered point to each square of size pixels: for every pixel the nearest point's index,
    the distance to it (F1) and to the next nearest (F2), in pixels."""
    k = N // size
    jitter = rng.uniform(0.1, 0.9, (k, k, 2)) * size
    ys, xs = np.mgrid[0:N, 0:N].astype(np.float64)
    # (the cells' edges shaken, so a clod is a lump and not a plate)
    ys = ys + warp[0] * size * 0.22
    xs = xs + warp[1] * size * 0.22
    cy = np.floor(ys / size).astype(int)
    cx = np.floor(xs / size).astype(int)
    f1 = np.full((N, N), 1e9)
    f2 = np.full((N, N), 1e9)
    near = np.zeros((N, N), dtype=np.int64)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ny = cy + dy
            nx = cx + dx
            wy = ny % k
            wx = nx % k
            py = ny * size + jitter[wy, wx, 0]
            px = nx * size + jitter[wy, wx, 1]
            d = np.hypot(ys - py, xs - px)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            near = np.where(closer, wy * k + wx, near)
            f1 = np.where(closer, d, f1)
    return near, f1, f2


def soil(rng):
    """Bare Normandy soil, a loam, 2 mm a pixel: clods and crumbs at three sizes, each a lump with a parting round
    it, grit and small flints, the dark of humus in the hollows, a few bits of straw. Dry on its tops, damp in its
    partings."""
    big = periodic_noise(rng, N, 4, 40, 1.4)
    height = 0.25 * big
    gap_dark = np.zeros((N, N))
    tone = np.zeros((N, N))
    # Clods at three sizes (about 6, 3 and 1.5 cm across): each cell a lump, round-topped, a dark parting round it.
    for size, lift, keep in ((32, 1.0, 0.55), (16, 0.6, 0.7), (8, 0.35, 0.85)):
        warp = (periodic_noise(rng, N, N // size, 4 * N // size, 0.5), periodic_noise(rng, N, N // size, 4 * N // size, 0.5))
        near, f1, f2 = cells(rng, size, warp)
        count = (N // size) ** 2
        lump = rng.uniform(0.5, 1.0, count)
        shown = rng.random(count) < keep
        # (clods gather where the broad noise is high: trodden flat elsewhere)
        here = shown[near] & (big > -0.8 + 0.6 * rng.random(count)[near])
        across = np.clip(2.0 * f1 / (f1 + f2), 0, 1)  # 0 at the clod's heart, 1 at its edge
        dome = np.sqrt(np.clip(1.0 - across ** 3, 0, 1)) * lump[near]
        height = np.where(here, np.maximum(height, 0.25 * big + lift * dome), height)
        gap_dark += np.where(here, 1.0 - np.clip((f2 - f1) / (0.06 * size), 0, 1), 0.0) * lift * 0.6
        tone += np.where(here, rng.normal(0, 0.07, count)[near] * lift, 0.0)
    crumb = periodic_noise(rng, N, 80, 400, 0.3)
    height = height + 0.06 * crumb
    # Brightness: tops dry and pale, hollows damp and dark (the height against its own blur), the partings darkest.
    relief = height - periodic_blur(height, 8.0)
    bright = 0.5 + 0.6 * relief + 0.05 * big + tone - 0.35 * np.clip(gap_dark, 0, 1) + 0.025 * crumb
    grit = rng.random((N, N))
    pale = grit > 0.988
    dark = grit < 0.01
    bright[pale] += 0.3
    bright[dark] -= 0.25
    # Small stones: flints, pale and cold, standing a little proud.
    stones = Image.new("L", (N, N), 0)
    sd = ImageDraw.Draw(stones)
    for _ in range(110):
        cx, cy = rng.uniform(0, N, 2)
        r = rng.uniform(1.5, 4.0)
        ellipse_wrapped(sd, cx, cy, r, r * rng.uniform(0.6, 1.0), int(rng.uniform(150, 255)), N)
    stone = np.asarray(stones) / 255.0
    bright = np.where(stone > 0, 0.75 + 0.25 * stone, bright)
    height = height + 0.5 * periodic_blur(stone, 0.8)
    # Straw: short, thin, pale stalks lying flat, a few.
    bits = Image.new("L", (N, N), 0)
    bd = ImageDraw.Draw(bits)
    for _ in range(260):
        x, y = rng.uniform(0, N, 2)
        a = rng.uniform(0, 2 * np.pi)
        length = rng.uniform(5, 14)
        draw_wrapped(bd, [(x, y), (x + np.cos(a) * length, y + np.sin(a) * length)], 255, 1, N, 30)
    straw = np.asarray(bits) > 0
    bright[straw] = 0.85 + 0.1 * rng.random(straw.sum())
    height[straw] += 0.1
    return height, bright


def patch_map(rng):
    """The broad lie of a field, about 40 m a repeat, 4 cm a pixel: nothing finer than about a third of a metre (the
    grain does the rest)."""
    broad = periodic_noise(rng, N, 2, 30, 1.1)
    # Tussocks and clumps (a third of a metre to a metre): the rank, darker, bluer grass the cattle leave. Kept apart
    # from value and warmth, in a channel of their own, so that the material can lay them on grass alone: on bare
    # soil, a cornfield's floor or a lane's dust they were an even rash of dark spots.
    clump_field = periodic_noise(rng, N, 40, 120, 0.6)
    clumps = np.clip(clump_field - 0.4, 0, None) ** 1.3
    value = 0.6 * broad + 0.15 * periodic_noise(rng, N, 30, 120, 0.5)
    warmth = 0.7 * periodic_noise(rng, N, 2, 24, 1.2) + 0.2 * periodic_noise(rng, N, 20, 80, 0.6)
    # Wear: trodden and grazed-bare places, a few metres across and ragged (a gateway, where the cattle stood, a
    # path), the grass not wholly gone inside them.
    warp_u = periodic_noise(rng, N, 4, 24, 1.0)
    warp_v = periodic_noise(rng, N, 4, 24, 1.0)
    big = periodic_noise(rng, N, 3, 12, 1.0)
    ys, xs = np.mgrid[0:N, 0:N]
    big = big[(ys + (warp_v * 28).astype(int)) % N, (xs + (warp_u * 28).astype(int)) % N]
    wear = np.clip((big - 1.35) / 0.6, 0, 1)
    within = periodic_noise(rng, N, 25, 100, 0.5)
    wear = periodic_blur(np.clip(wear * (0.75 + 0.3 * within), 0, 1), 2.0)
    # (where it is worn it is drier and paler: trodden ground is lighter and yellower than grass)
    warmth = warmth + 0.6 * wear
    # Where the tussocks gather (a few metres to a dozen across): thick in some places, a few in others, none at all
    # in about a third of the field, so they are never an even scatter. None where the ground is worn bare.
    gather = np.clip(0.4 + 0.55 * periodic_noise(rng, N, 2, 10, 1.0), 0, 1) ** 1.5
    tussocks = periodic_blur(clumps, 0.7) * gather * (1.0 - wear)
    tussocks = np.clip(tussocks / np.percentile(tussocks, 99.7), 0, 1)
    return centred(value, 0.2), centred(warmth, 0.17), np.clip(wear, 0, 1), tussocks


def save(path, channels):
    pixels = np.stack(channels, axis=-1)
    Image.fromarray(pixels, "RGBA" if len(channels) == 4 else "RGB").save(path, optimize=True)
    print(f"wrote {os.path.relpath(path, HERE)}: {os.path.getsize(path) / 1e6:.2f} MB")


def main():
    rng = np.random.default_rng(SEED)
    g_height, g_bright, g_hue, gaps = sward(rng)
    s_height, s_bright = soil(rng)
    print(f"sward: {gaps * 100:.0f}% of the ground between blades")

    # Grain: each channel to mean 0.5, so the vertex colour is the field's average colour as before.
    grass_lum = centred(g_bright, 0.17)
    soil_lum = centred(s_bright, 0.1)
    grass_hue = centred(g_hue, 0.15)
    # (6 bits are plenty for a grain that is multiplied in, 5 for its hue; it keeps the file small)
    save(os.path.join(HERE, "T_LandGrain.png"), [coarse(grass_lum, 6), coarse(soil_lum, 6), coarse(grass_hue, 5)])

    # Lean: from the heights softened by a pixel (a blade is round in section, a clod's edge is not a cliff).
    gu, gv = lean_of(periodic_blur(g_height, 1.0), 2.2)
    su, sv = lean_of(periodic_blur(s_height, 0.8), 1.0)
    # (6 bits: a lean of 1 in 32 is finer than the eye reads in a detail this small)
    save(os.path.join(HERE, "T_LandBump.png"), [coarse(0.5 + 0.5 * gu, 6), coarse(0.5 + 0.5 * gv, 6), coarse(0.5 + 0.5 * su, 6), coarse(0.5 + 0.5 * sv, 6, 4)])

    value, warmth, wear, tussocks = patch_map(rng)
    print(f"tussocks: on {(tussocks > 0.1).mean() * 100:.0f}% of the field")
    save(os.path.join(HERE, "T_LandPatch.png"), [byte(value), byte(warmth), byte(wear), byte(tussocks, 1)])


if __name__ == "__main__":
    main()
