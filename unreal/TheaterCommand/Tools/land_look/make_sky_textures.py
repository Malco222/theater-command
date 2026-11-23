"""
Paints the ground battle's picture of the air over the field, which Tools/build_content.py brings in for two things:
M_LandCloudShade, the light function on the battle's sun (the shadows of a fair day's cumulus lying on the field and
sliding over it with the wind), and the wind's gusts that bend the grass, the corn and the trees (M_LandBlades,
M_LandLeaves, M_LandStalks) and run over the fields as a sheen (M_LandSurface). Made from numbers with a fixed seed:
nothing is fetched and nothing copied.

    python3 Tools/land_look/make_sky_textures.py        (numpy and Pillow; a second or two)

writes, beside this script:

T_LandCloudWind.png    512 px. Linear numbers, not colours, each ranked evenly from 0 to 1 (the share of the picture
                       above 1 - c is c).
                       R  the clouds, about 4 km a repeat (the material's "ShadeSize"): how cloudy the sky is over a
                          place. The material shades a place where it is over 1 - Cover, so the day's cover of
                          cumulus casts that share of the field into shadow. Clouds of some 300 m to 1.5 km, gathered
                          in loose groups with clear sky between, their edges a little ragged (100 to 200 m).
                       G  the gusts, about 150 m a repeat (the materials' "GustSize"): how hard the wind blows over a
                          place, carried downwind at the gusts' own speed. Gusts of some 20 to 75 m, a few stronger.
                       B  as R, A 255.

It repeats without a seam: the noise is made periodic in the frequency domain. Alpha is never 0, because Unreal's PNG
import fills the colour of fully clear pixels.
"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
N = 512
SEED = 19440606


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


def cloud_shade():
    rng = np.random.default_rng(SEED)
    # (the groups: a few across the 4 km, where the clouds gather and where the sky stays clear)
    groups = periodic_noise(rng, N, 1.5, 5.0, power=1.0)
    # (the clouds themselves, some 300 m to 1.5 km: 3 to 14 across the repeat)
    clouds = periodic_noise(rng, N, 3.0, 14.0, power=1.7)
    # (their edges a little ragged, 100 to 200 m)
    rags = periodic_noise(rng, N, 20.0, 40.0, power=1.0)
    sky = 0.6 * groups + 1.0 * clouds + 0.09 * rags
    return ranked(sky)


def gusts():
    rng = np.random.default_rng(SEED + 1)
    # (2 to 8 across the 150 m: gusts of 20 to 75 m, their shapes soft)
    return ranked(periodic_noise(rng, N, 2.0, 8.0, power=1.5))


def save(name, red, green):
    r = np.clip(np.round(red * 255.0), 0, 255).astype(np.uint8)
    g = np.clip(np.round(green * 255.0), 0, 255).astype(np.uint8)
    rgba = np.dstack([r, g, r, np.full_like(r, 255)])
    Image.fromarray(rgba, "RGBA").save(os.path.join(HERE, name))
    print(f"wrote {name}: {red.shape[1]} x {red.shape[0]}, clouds mean {red.mean():.3f}, gusts mean {green.mean():.3f}")


if __name__ == "__main__":
    save("T_LandCloudWind.png", cloud_shade(), gusts())
