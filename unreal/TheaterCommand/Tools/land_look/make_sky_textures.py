"""
Paints the ground battle's picture of the air over the field. Tools/build_content.py brings it in for two things:
M_LandCloudShade, the light function on the battle's sun, which lays a fair day's cumulus shadows on the field and slides
them over it with the wind; and the wind's gusts, which bend the grass, the corn and the trees (M_LandBlades,
M_LandLeaves, M_LandStalks) and run over the fields as a sheen (M_LandSurface). It is made from numbers with a fixed
seed: nothing is fetched and nothing is copied.

    python3 Tools/land_look/make_sky_textures.py        (numpy and Pillow; a few seconds)

It writes T_LandCloudWind.png beside this script: 1024 px, linear numbers, not colours.

R  the clouds' shadows, 4 km a repeat (the material's "ShadeSize"). Each texel holds how far it lies inside the nearest
   cloud's shadow, in metres: 0.5 + metres / (2 * REACH), clamped, so 0.5 is the edge, above is under a cloud and
   below is in the sun. The material blurs the edge over "Feather" metres, and because the value is a distance that
   blur is the same width all the way round every cloud.
   The clouds are cumulus seen from below, after the art director's round 1 of 23 November. Each is 150 to 600 m
   across and lobed: a body with four to eight rounder lobes on its rim, joined by soft fillets, with edges a little
   ragged. They lie in loose groups with sun between, and never touch, so each keeps its own outline. In all they shade
   FULL_COVER of the ground. A disc of CLEAR_RADIUS metres round CLEAR_UV is left without cloud: the battle's
   C++ lays it over the middle of the field at the start, so the opening shot stands in sun and the shadows cross the
   rest.
B  each cloud's place in the order the day's cover keeps them (0..1, the same over the whole cloud). The material
   shades only clouds with B under Cover / FULL_COVER, so a day of less cover has fewer clouds, not smaller ones.
G  the gusts, about 150 m a repeat (the materials' "GustSize"): how hard the wind blows over a place, carried downwind
   at the gusts' own speed. Gusts are some 20 to 75 m across, a few of them stronger. Ranked evenly from 0 to 1, so the
   share of the picture above 1 - c is c.
A  255.

The picture repeats without a seam: the gusts are periodic in the frequency domain, and the clouds are measured round
the repeat. Alpha is never 0, because Unreal's PNG import fills in the colour of fully clear pixels.
"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
N = 1024
SEED = 19440606
SIZE = 4000.0          # metres a repeat (M_LandCloudShade's ShadeSize)
REACH = 400.0          # metres either side of the edge that R holds
CLEAR_UV = (0.5, 0.5)  # the clear disc's middle, in repeats (M_LandCloudShade's "Clear")
CLEAR_RADIUS = 280.0   # metres
GAP = 40.0             # metres of sun at least between two clouds
RING = 6               # clouds laid round the clear disc


def periodic_noise(rng, n, low, high, power=1.0):
    """Noise that repeats over n pixels, holding wavelengths between n/high and n/low pixels, with an amplitude
    falling as 1/f**power. Normalised to mean 0, standard deviation 1. (As make_ground_textures.py's.)"""
    white = rng.standard_normal((n, n))
    f = np.fft.fftfreq(n) * n
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx * fx + fy * fy)
    band = np.where((r >= low) & (r <= high), 1.0 / np.maximum(r, 1.0) ** power, 0.0)
    band *= np.clip((r - low + 1.0), 0, 1) * np.clip((high - r + 1.0), 0, 1)
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * band))
    out -= out.mean()
    return out / (out.std() + 1e-9)


def ranked(x):
    """The same order, spread evenly over 0..1 (so a threshold at 1 - c leaves the share c above it)."""
    order = np.argsort(x, axis=None)
    out = np.empty(x.size, dtype=np.float64)
    out[order] = (np.arange(x.size) + 0.5) / x.size
    return out.reshape(x.shape)


def smooth_max(a, b, k):
    """The greater of a and b, rounded over k where they are near: the fillet where a lobe joins its cloud."""
    h = np.clip(k - np.abs(a - b), 0.0, None) / k
    return np.maximum(a, b) + h * h * k * 0.25


def clouds(rng):
    """The cumulus: (inside, order), each N x N. inside is in metres, positive under a cloud. order is the cloud's
    place, the same over the whole cloud and its sunlit surround."""
    px = SIZE / N
    groups = periodic_noise(rng, N, 1.5, 5.0, power=1.0)          # where the clouds gather (a few across the 4 km)
    rags = periodic_noise(rng, N, SIZE / 60.0, SIZE / 25.0, 1.0)   # edges a little ragged at 25 to 60 m
    placed = []   # (x, y, reach, parts)
    at = np.zeros((0, 3))  # (x, y, reach) of those placed, to test a new one against all at once
    cx, cy = CLEAR_UV[0] * SIZE, CLEAR_UV[1] * SIZE
    def cumulus(extent):
        """A cumulus from below, `extent` metres from its middle to its rim: a body, five to nine lobes round its rim
        reaching out to the extent, and on each lobe two smaller ones, so the outline is a cauliflower's, not an oval's;
        some drawn out along an axis. (parts, reach): parts [(x, y, r)] from its middle."""
        body = extent * rng.uniform(0.5, 0.62)
        parts = [(0.0, 0.0, body)]
        lobes = int(rng.integers(5, 10))
        turn = rng.uniform(0.0, 2.0 * np.pi)
        for k in range(lobes):
            a = turn + 2.0 * np.pi * (k + rng.uniform(-0.3, 0.3)) / lobes
            d = body * rng.uniform(0.55, 0.85)
            r = max(extent * rng.uniform(0.78, 1.0) - d, 0.3 * body)
            lx, ly = d * np.cos(a), d * np.sin(a)
            parts.append((lx, ly, r))
            for side in (-1.0, 1.0):
                b = a + side * rng.uniform(0.35, 0.8)
                parts.append((lx + 0.78 * r * np.cos(b), ly + 0.78 * r * np.sin(b), r * rng.uniform(0.3, 0.45)))
        stretch, axis = rng.uniform(1.0, 1.45), rng.uniform(0.0, np.pi)
        ca, sa = np.cos(axis), np.sin(axis)
        parts = [((qx * ca + qy * sa) * stretch * ca - (-qx * sa + qy * ca) * sa,
                  (qx * ca + qy * sa) * stretch * sa + (-qx * sa + qy * ca) * ca, r) for qx, qy, r in parts]
        return parts, max(np.hypot(qx, qy) + r for qx, qy, r in parts) + 6.0

    def room(x, y, reach):
        """Whether a cloud may stand there: off the clear disc, and GAP from every other."""
        dx = (x - cx + SIZE / 2) % SIZE - SIZE / 2
        dy = (y - cy + SIZE / 2) % SIZE - SIZE / 2
        if np.hypot(dx, dy) < CLEAR_RADIUS + reach:
            return False
        if len(at):
            dx = (x - at[:, 0] + SIZE / 2) % SIZE - SIZE / 2
            dy = (y - at[:, 1] + SIZE / 2) % SIZE - SIZE / 2
            if np.any(np.hypot(dx, dy) < reach + at[:, 2] + GAP):
                return False
        return True

    # A ring of clouds just off the clear disc, all round it (the art director's round 2: the opening's first frame
    # shows cloud country, one or two patches on its flanks or far fields, whichever way it looks, the middle still in
    # sun). They are first in the day's order, so every day with clouds has them.
    ring = 0
    turn = rng.uniform(0.0, 2.0 * np.pi)
    for k in range(RING):
        parts, reach = cumulus(rng.uniform(120.0, 210.0))
        for _ in range(40):
            a = turn + 2.0 * np.pi * (k + rng.uniform(-0.25, 0.25)) / RING
            d = CLEAR_RADIUS + reach + rng.uniform(10.0, 90.0)
            x, y = (cx + d * np.cos(a)) % SIZE, (cy + d * np.sin(a)) % SIZE
            if room(x, y, reach):
                placed.append((x, y, reach, parts))
                at = np.vstack([at, [x, y, reach]])
                ring += 1
                break
    # (the rest: more small clouds than big, 150 to 600 m across; laid biggest first, so the small ones fill the sun
    # between)
    extents = np.sort(75.0 * (300.0 / 75.0) ** (rng.uniform(size=900) ** 1.5))[::-1]
    for extent in extents:
        parts, reach = cumulus(extent)
        for _ in range(80):
            x, y = rng.uniform(0.0, SIZE, 2)
            # (gathered loosely in groups: fewer where the groups' noise is low)
            g = groups[int(y / px) % N, int(x / px) % N]
            if rng.uniform() > 1.0 / (1.0 + np.exp(-2.2 * (g + 0.3))):
                continue
            if room(x, y, reach):
                placed.append((x, y, reach, parts))
                at = np.vstack([at, [x, y, reach]])
                break
    # (the order the day's cover keeps them in: the ring first, then the rest at random, so a thin day's few clouds
    # are as spread as a full day's)
    orders = np.concatenate([rng.permutation(ring), ring + rng.permutation(len(placed) - ring)]).astype(np.float64)
    orders = (orders + 0.5) / len(placed)
    inside = np.full((N, N), -REACH * 4.0)
    order = np.zeros((N, N))
    best = np.full((N, N), -1e9)
    for (x, y, reach, parts), o in zip(placed, orders):
        span = reach + REACH
        i0, i1 = int(np.floor((x - span) / px)), int(np.ceil((x + span) / px))
        j0, j1 = int(np.floor((y - span) / px)), int(np.ceil((y + span) / px))
        ii = np.arange(i0, i1)
        jj = np.arange(j0, j1)
        X, Y = np.meshgrid((ii + 0.5) * px - x, (jj + 0.5) * px - y)
        field = None
        for (qx, qy, r) in parts:
            d = r - np.hypot(X - qx, Y - qy)
            field = d if field is None else smooth_max(field, d, 18.0)
        rows = jj % N
        cols = ii % N
        R, C = np.meshgrid(rows, cols, indexing="ij")
        better = field > best[R, C]
        best[R[better], C[better]] = field[better]
        order[R[better], C[better]] = o
        inside[R, C] = np.maximum(inside[R, C], field)
    inside = inside + 4.0 * rags * np.clip(1.0 - np.abs(inside) / 60.0, 0.0, 1.0)
    return inside, order, len(placed)


def gusts(rng):
    # (2 to 8 across the 150 m: gusts of 20 to 75 m, their shapes soft)
    return ranked(periodic_noise(rng, N, 2.0, 8.0, power=1.5))


def save(name, inside, order, wind):
    red = np.clip(0.5 + inside / (2.0 * REACH), 0.0, 1.0)
    r = np.clip(np.round(red * 255.0), 0, 255).astype(np.uint8)
    b = np.clip(np.round(order * 255.0), 0, 255).astype(np.uint8)
    g = np.clip(np.round(wind * 255.0), 0, 255).astype(np.uint8)
    rgba = np.dstack([r, g, b, np.full_like(r, 255)])
    Image.fromarray(rgba, "RGBA").save(os.path.join(HERE, name))


if __name__ == "__main__":
    inside, order, count = clouds(np.random.default_rng(SEED))
    wind = gusts(np.random.default_rng(SEED + 1))
    save("T_LandCloudWind.png", inside, order, wind)
    full = float((inside > 0.0).mean())
    print(f"wrote T_LandCloudWind.png: {N} x {N}, {count} clouds shading {full:.3f} of the ground (FULL_COVER), "
          f"the clear disc {CLEAR_RADIUS:.0f} m round {CLEAR_UV}, gusts mean {wind.mean():.3f}")
    for keep in (0.25, 0.5, 0.75, 1.0):
        share = float(((inside > 0.0) & (np.round(order * 255.0) / 255.0 < keep)).mean())
        print(f"  cover asked {keep * full:.3f}: shaded {share:.3f}")
