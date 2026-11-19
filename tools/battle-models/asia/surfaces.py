"""Ground paint for the Asian sets: tileable colour pictures with the ground's height in the alpha.

    python surfaces.py <out folder> [size=512]

Every picture wraps at its edges (made from noise that repeats, and every scattered mark is drawn at its wrapped
places too), so the game lays them edge to edge. Colours start from the art director's world-map table
(world-look-brief.md 3.3) and break them into the few tones the ground really shows at a man's height: leaf litter,
grass tufts, wet patches, furrows, cracks. Nothing is copied from a photograph.
"""
import os
import sys

import numpy as np
from PIL import Image


def hexrgb(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float) / 255.0


def fbm(n, rng, beta=2.0, lo=2.0, hi=None):
    """Noise that repeats at the edges: white noise shaped to a 1/f^beta spectrum, scaled 0..1."""
    w = rng.normal(size=(n, n))
    fx = np.fft.fftfreq(n) * n
    f = np.sqrt(fx[:, None] ** 2 + fx[None, :] ** 2)
    f[0, 0] = 1.0
    amp = 1.0 / f ** (beta / 2)
    amp[f < lo] *= (f[f < lo] / lo) ** 2
    if hi:
        amp[f > hi] = 0
    out = np.real(np.fft.ifft2(np.fft.fft2(w) * amp))
    out -= out.min()
    return out / out.max()


def ramp(t, stops):
    """t (n,n) 0..1 through colour stops [(t, hex), ...]."""
    ts = np.array([s for s, _ in stops])
    cs = np.array([hexrgb(c) for _, c in stops])
    out = np.zeros(t.shape + (3,))
    for k in range(3):
        out[..., k] = np.interp(t, ts, cs[:, k])
    return out


def stamp(img, hgt, rng, count, size_px, colours, shape="leaf", height=0.1, angle=None, alpha=0.9):
    """Scatter small marks (leaves, tufts, pebbles) over the picture, wrapped at the edges."""
    n = img.shape[0]
    size_px = max(2, int(round(size_px)))
    yy, xx = np.mgrid[-size_px:size_px + 1, -size_px:size_px + 1]
    for _ in range(count):
        cx, cy = rng.integers(0, n, 2)
        a = rng.uniform(0, np.pi) if angle is None else angle + rng.normal() * 0.25
        s = size_px * rng.uniform(0.55, 1.0)
        u = (xx * np.cos(a) + yy * np.sin(a)) / s
        v = (-xx * np.sin(a) + yy * np.cos(a)) / s
        if shape == "leaf":
            m = (u ** 2 + (v / 0.42) ** 2) < 1.0
        elif shape == "tuft":
            m = (np.abs(v) < 0.18 * (1 - np.abs(u))) & (np.abs(u) < 1)
        else:                                            # pebble
            m = (u ** 2 + (v / 0.75) ** 2) < 1.0
        c = hexrgb(colours[rng.integers(len(colours))]) * rng.uniform(0.88, 1.1)
        ys, xs = np.nonzero(m)
        Y = (cy + ys - size_px) % n
        X = (cx + xs - size_px) % n
        img[Y, X] = img[Y, X] * (1 - alpha) + c * alpha
        hgt[Y, X] = np.maximum(hgt[Y, X], hgt[Y, X] * 0.5 + height)


def cells(n, rng, count):
    """Distance to the nearest cell edge of a wrapped Voronoi pattern (for dried mud's cracks)."""
    k = max(2, int(round(count ** 0.5)))                    # a jittered grid: plates of like size, no clusters
    g = (np.mgrid[0:k, 0:k].reshape(2, -1).T + 0.5 + rng.uniform(-0.4, 0.4, (k * k, 2))) * (n / k)
    P = g % n
    yy, xx = np.mgrid[0:n, 0:n].astype(float)
    d1 = np.full((n, n), 1e9)
    d2 = np.full((n, n), 1e9)
    for px, py in P:
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        dx = np.minimum(dx, n - dx)
        dy = np.minimum(dy, n - dy)
        d = np.sqrt(dx * dx + dy * dy)
        d2 = np.where(d < d1, d1, np.minimum(d2, d))
        d1 = np.minimum(d1, d)
    return d2 - d1


# Each surface: (colour of the ground as the map sees it, metres a picture covers, maker)
def jungle_floor(n, rng):
    t = fbm(n, rng, 2.2)
    img = ramp(t, [(0, "3A3624"), (0.5, "4A4630"), (1, "5C5434")])
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 90), n / 70, ["6A5434", "7A6038", "5A4A2E", "8A6E40", "4E5A30"], "leaf", 0.35)
    stamp(img, hgt, rng, int(n * n / 900), n / 40, ["3E5A30", "4A6634", "34502C"], "leaf", 0.5)
    return img, hgt


def laterite(n, rng):
    t = fbm(n, rng, 2.4)
    img = ramp(t, [(0, "7C4C34"), (0.5, "985E40"), (1, "AA7050")])
    hgt = t * 0.4
    stamp(img, hgt, rng, int(n * n / 700), n / 110, ["6E4430", "B48064", "8C5A40"], "pebble", 0.6)
    return img, hgt


def laterite_grass(n, rng):
    t = fbm(n, rng, 2.0)
    m = fbm(n, rng, 2.6, lo=1.0)
    img = ramp(t, [(0, "5E6A3C"), (0.6, "687E4C"), (1, "7A8A52")])
    earth = ramp(t, [(0, "8C5A3C"), (1, "A06C4C")])
    k = np.clip((m - 0.62) * 6, 0, 1)[..., None]
    img = img * (1 - k) + earth * k
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 60), n / 45, ["6A7E44", "7E8E4E", "8E9658", "56683A", "A09A60"], "tuft", 0.5)
    return img, hgt


def paddy_mud(n, rng):
    t = fbm(n, rng, 2.2)
    w = fbm(n, rng, 2.8, lo=1.0)
    img = ramp(t, [(0, "4E4636"), (0.5, "625844"), (1, "746A52")])
    water = ramp(t, [(0, "6E7A6C"), (1, "86948A")])
    k = np.clip((w - 0.68) * 5, 0, 1)[..., None] * 0.85
    img = img * (1 - k) + water * k
    hgt = t * 0.2 * (1 - k[..., 0] * 0.8)
    stamp(img, hgt, rng, int(n * n / 1500), n / 60, ["7A8A48", "8A9650", "6A7A40"], "tuft", 0.3)
    return img, hgt


def loess(n, rng):
    t = fbm(n, rng, 2.4)
    img = ramp(t, [(0, "947E58"), (0.5, "AA946E"), (1, "BAA47E")])
    hgt = t * 0.35
    stamp(img, hgt, rng, int(n * n / 1100), n / 80, ["7E7650", "6E7044", "8E8456", "76724A"], "tuft", 0.4)
    return img, hgt


def loess_furrows(n, rng):
    """A ploughed loess field: furrows a quarter metre apart running across the picture, stubble on the ridges."""
    t = fbm(n, rng, 2.4)
    y = np.mgrid[0:n, 0:n][0] / n
    rows = 24
    wob = (fbm(n, rng, 3.0, lo=1.0) - 0.5) * 0.02
    f = 0.5 + 0.5 * np.cos((y + wob) * rows * 2 * np.pi)
    img = ramp(t * 0.4 + f * 0.6, [(0, "8C7A56"), (0.5, "A8946C"), (1, "BEAA80")])
    hgt = f * 0.7 + t * 0.3
    stamp(img, hgt, rng, int(n * n / 400), n / 160, ["C4B48A", "9A8A5E"], "tuft", 0.8, angle=0.0)
    return img, hgt


def gaoliang_stubble(n, rng):
    """A gaoliang field out of season: the high ridges it is sown on, 0.6 m apart, cut stalks standing in the rows
    (the field the gaoliang stands on from June to September)."""
    t = fbm(n, rng, 2.4)
    y = np.mgrid[0:n, 0:n][0] / n
    rows = 10
    wob = (fbm(n, rng, 3.0, lo=1.0) - 0.5) * 0.015
    f = (0.5 + 0.5 * np.cos((y + wob) * rows * 2 * np.pi)) ** 1.6
    img = ramp(t * 0.35 + f * 0.65, [(0, "7E6C4C"), (0.5, "9A8662"), (1, "B09C76")])
    hgt = f * 0.8 + t * 0.2
    for k in range(rows):                                # the stalks stand on the ridge tops
        yc = (k / rows) * n
        m = int(n / 9)
        xs = rng.uniform(0, n, m)
        r = max(1, int(n / 300))
        for x in xs:
            cx, cy = int(x) % n, int(yc + rng.normal() * n / 240) % n
            ii = np.arange(cy - r, cy + r + 1) % n                # (wrapped, so the picture still tiles)
            jj = np.arange(cx - r, cx + r + 1) % n
            img[np.ix_(ii, jj)] = hexrgb("C8B88C" if rng.random() < 0.6 else "8A7A52")
    stamp(img, hgt, rng, int(n * n / 900), n / 120, ["B8A87C", "8A7A52"], "tuft", 0.5)
    return img, hgt


def alluvial(n, rng):
    """The Tigris and Euphrates plain: grey-brown silt dried into plates, a little salt white in the hollows."""
    t = fbm(n, rng, 2.2)
    e = cells(n, rng, 70)
    img = ramp(t, [(0, "84765C"), (0.5, "9A8A6C"), (1, "AC9C7C")])
    crack = np.clip(1 - e / 2.2, 0, 1)[..., None]
    img = img * (1 - crack * 0.55)
    salt = np.clip((fbm(n, rng, 2.6, lo=1.0) - 0.75) * 5, 0, 1)[..., None]
    img = img * (1 - salt * 0.35) + hexrgb("D4CCB8") * salt * 0.35
    hgt = t * 0.3 + np.clip(e / 8, 0, 1) * 0.5
    return img, hgt


def scree_wet(n, rng):
    t = fbm(n, rng, 2.0)
    img = ramp(t, [(0, "5E5E58"), (0.5, "76766E"), (1, "8E8E86")])
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 260), n / 50, ["6E6E68", "84847C", "5A5A54", "9A9A90"], "pebble", 0.8)
    stamp(img, hgt, rng, int(n * n / 500), n / 60, ["56663A", "5E6E40"], "tuft", 0.4)
    return img, hgt


def scree_dry(n, rng):
    t = fbm(n, rng, 2.0)
    img = ramp(t, [(0, "8A7058"), (0.5, "A48668"), (1, "B89C7E")])
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 260), n / 50, ["9A7E62", "B0947A", "7E6650", "C0A88A"], "pebble", 0.8)
    stamp(img, hgt, rng, int(n * n / 2500), n / 45, ["6E6A44", "7A7448"], "tuft", 0.4)
    return img, hgt


def hill_grass(n, rng):
    t = fbm(n, rng, 2.2)
    img = ramp(t, [(0, "4E5E34"), (0.5, "5E6E3C"), (1, "6E7C46")])
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 70), n / 50, ["5E7040", "6E7E48", "7E8A50", "4A5A30"], "tuft", 0.5)
    return img, hgt


def zagros_scrub(n, rng):
    t = fbm(n, rng, 2.2)
    img = ramp(t, [(0, "8E7C5C"), (0.5, "A08E68"), (1, "B09E78")])
    hgt = t * 0.3
    stamp(img, hgt, rng, int(n * n / 400), n / 70, ["8A7660", "B8A484"], "pebble", 0.5)
    stamp(img, hgt, rng, int(n * n / 6000), n / 40, ["6A6444", "746E4A", "5E5A3E"], "pebble", 0.6, alpha=0.7)
    stamp(img, hgt, rng, int(n * n / 300), n / 90, ["7A7048", "8A7E56"], "tuft", 0.3, alpha=0.6)
    return img, hgt


SURFACES = {
    # name: (maker, metres a picture covers, the ground's colour as one value for the far view)
    "jungle_floor": (jungle_floor, 6.0),
    "laterite": (laterite, 6.0),
    "laterite_grass": (laterite_grass, 6.0),
    "paddy_mud": (paddy_mud, 8.0),
    "loess": (loess, 8.0),
    "loess_furrows": (loess_furrows, 6.0),
    "gaoliang_stubble": (gaoliang_stubble, 6.0),
    "alluvial": (alluvial, 8.0),
    "scree_wet": (scree_wet, 6.0),
    "scree_dry": (scree_dry, 6.0),
    "hill_grass": (hill_grass, 6.0),
    "zagros_scrub": (zagros_scrub, 8.0),
}


def make(name, n=512, seed=None):
    fn, tile = SURFACES[name]
    rng = np.random.default_rng(seed if seed is not None else abs(hash(name)) % (1 << 30))
    img, hgt = fn(n, rng)
    hgt = (hgt - hgt.min()) / max(1e-6, np.ptp(hgt))
    rgba = np.concatenate([np.clip(img, 0, 1), hgt[..., None]], axis=2)
    mean = np.clip(img.reshape(-1, 3).mean(0), 0, 1)
    return (rgba * 255 + 0.5).astype(np.uint8), tile, "".join(f"{int(v * 255 + 0.5):02X}" for v in mean)


def main():
    out = sys.argv[1]
    opts = dict(a.split("=", 1) for a in sys.argv[2:] if "=" in a)
    n = int(opts.get("size", 512))
    os.makedirs(out, exist_ok=True)
    for name in SURFACES:
        rgba, tile, mean = make(name, n, seed=sum(map(ord, name)))
        Image.fromarray(rgba, "RGBA").save(os.path.join(out, f"{name}.png"), optimize=True)
        print(f"{name}: {tile} m, mean {mean}")


if __name__ == "__main__":
    main()
