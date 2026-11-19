"""The ground paints of the Mediterranean sets: tileable colour pictures with a height in the alpha, as the sets'
format asks (battle-models/asia/SET-FORMAT.md). Made from numbers here: soft noise that wraps at its edges, stones
laid as small domes, ruts and furrows as waves with a whole number of periods across the tile. No photo, no outside
texture.

    python surfaces.py <out folder> [name,name,...|all] [size]

Colours as seen, in the campaign map's palette (world-look brief 3.3; the art director's desert call of 19 Nov:
the default desert field is the stony 170, 140, 106 with patches of fine sand).
"""
import os
import sys

import numpy as np
from PIL import Image


def hexc(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float32)


def fbm(n, beta=1.8, seed=1, low=2):
    """Soft noise that tiles: random phases, an amplitude falling with frequency, the lowest `low` rings left out."""
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(n) * n
    fx, fy = np.meshgrid(f, f)
    k = np.sqrt(fx ** 2 + fy ** 2)
    amp = np.where(k >= low, 1.0 / np.maximum(k, 1.0) ** beta, 0.0)
    spec = amp * np.exp(2j * np.pi * rng.random((n, n)))
    out = np.real(np.fft.ifft2(spec))
    out -= out.min()
    return out / max(out.max(), 1e-9)


def stones(n, count, r_px, palette, seed=2, squash=0.8):
    """Stones as little domes laid at random on a tile that wraps: returns (colour, height, cover)."""
    rng = np.random.default_rng(seed)
    col = np.zeros((n, n, 3), dtype=np.float32)
    hgt = np.zeros((n, n), dtype=np.float32)
    cov = np.zeros((n, n), dtype=np.float32)
    for _ in range(count):
        cx, cy = rng.random(2) * n
        r = r_px[0] + (r_px[1] - r_px[0]) * rng.random() ** 2
        ang = rng.random() * np.pi
        sq = squash + (1 - squash) * rng.random()
        c = palette[rng.integers(len(palette))] * (0.9 + 0.2 * rng.random())
        R = int(r + 2)
        xs = (np.arange(int(cx) - R, int(cx) + R + 1)) % n
        ys = (np.arange(int(cy) - R, int(cy) + R + 1)) % n
        dx = (np.arange(int(cx) - R, int(cx) + R + 1) - cx)[None, :]
        dy = (np.arange(int(cy) - R, int(cy) + R + 1) - cy)[:, None]
        u = (dx * np.cos(ang) + dy * np.sin(ang)) / r
        v = (-dx * np.sin(ang) + dy * np.cos(ang)) / (r * sq)
        d = np.sqrt(u ** 2 + v ** 2)
        dome = np.clip(1 - d ** 2, 0, 1)
        m = (dome > 0).astype(np.float32) * np.clip((1 - d) * 4, 0, 1)
        shade = 0.82 + 0.3 * np.clip(-u * 0.6 - v * 0.6 + 0.5, 0, 1)      # (lit from one side, as a stone is)
        sub = np.ix_(ys, xs)
        h = dome * r / max(r_px[1], 1)
        top = h > hgt[sub]
        hgt[sub] = np.where(top, h, hgt[sub])
        col[sub] = np.where((top & (m > 0))[..., None], c[None, None, :] * shade[..., None], col[sub])
        cov[sub] = np.maximum(cov[sub], m * top)
    return col, hgt, cov


def ripple(n, periods, seed=3, wobble=0.12):
    """Wind ripples: waves across the tile, a whole number of them so the edges meet, bent by soft noise."""
    y, x = np.mgrid[0:n, 0:n] / n
    w = fbm(n, 2.2, seed, low=1)
    ph = 2 * np.pi * (periods * x + wobble * periods * (w - 0.5) + 2 * (y + 0.2 * np.sin(2 * np.pi * y)))
    return 0.5 + 0.5 * np.sin(ph) ** 3


def furrows(n, periods, seed=4, wobble=0.05):
    y, x = np.mgrid[0:n, 0:n] / n
    w = fbm(n, 2.4, seed, low=1)
    return 0.5 + 0.5 * np.sin(2 * np.pi * (periods * y + wobble * periods * (w - 0.5)))


def base(n, a, b, seed, beta=1.8, lo=0.3, hi=0.7):
    t = np.clip((fbm(n, beta, seed) - lo) / (hi - lo), 0, 1)[..., None]
    return hexc(a) * (1 - t) + hexc(b) * t


def tone(n, seed, amount=0.12, beta=1.2):
    return (1 - amount + 2 * amount * fbm(n, beta, seed, low=6))[..., None]


def lay(under, under_h, over, over_h, cover):
    c = cover[..., None]
    return under * (1 - c) + over * c, np.maximum(under_h, over_h * cover)


def gravel(n):
    """The stony desert: a crust of small stones, dark where the sun has varnished them, on a paler grit."""
    col = base(n, "AE906C", "A08262", 11) * tone(n, 12)
    h = fbm(n, 1.6, 13) * 0.15
    for count, r, pal, sd in ((int(n * n / 260), (1.4, 3.4), ("4E4034", "6E5A44", "7E6A50", "9A8262", "B8A27C"), 14),
                              (int(n * n / 4200), (4.0, 8.5), ("5A4A3A", "7A644A", "927A5C", "AE9672"), 15)):
        sc, sh, cv = stones(n, count, r, [hexc(p) for p in pal], sd)
        col, h = lay(col, h, sc, sh, cv)
    return col, h


def sand(n):
    """Fine sand blown into patches and drifts: smooth, pale, a faint ripple, the odd stone showing."""
    rp = ripple(n, 9, 21)
    col = base(n, "D8BE8C", "CCB080", 22) * (0.94 + 0.08 * rp[..., None]) * tone(n, 23, 0.05)
    h = 0.25 * rp + 0.1 * fbm(n, 1.8, 24)
    sc, sh, cv = stones(n, int(n * n / 9000), (1.4, 3.0), [hexc(p) for p in ("6E5A44", "8C7458", "A08868")], 25)
    return lay(col, h, sc, sh * 0.6, cv)


def wadi_floor(n):
    """A wadi's floor: silt the last rain left, pale and fine, dried into faint plates, pebbles along it."""
    col = base(n, "CDB58C", "BFA67E", 31) * tone(n, 32, 0.07)
    plates = fbm(n, 1.1, 33, low=10)
    crack = np.clip(1 - np.abs(plates - 0.5) * 30, 0, 1)
    col *= (1 - 0.18 * crack)[..., None]
    h = 0.2 * fbm(n, 1.8, 34) - 0.15 * crack
    sc, sh, cv = stones(n, int(n * n / 1400), (1.6, 4.5), [hexc(p) for p in ("8C7458", "A89070", "BCA684", "6E5A44")], 35)
    col, h = lay(col, h, sc, sh, cv)
    return col, h


def escarp_rock(n):
    """The escarpment's cap and broken rock: pale limestone in beds, darker where it weathers, pitted."""
    y = np.mgrid[0:n, 0:n][0] / n
    beds = 0.5 + 0.5 * np.sin(2 * np.pi * (6 * y + 0.8 * (fbm(n, 2.0, 41, low=1) - 0.5)))
    col = (hexc("9A8262") * (1 - beds[..., None]) + hexc("B4A07C") * beds[..., None]) * tone(n, 42, 0.14)
    pits = fbm(n, 0.9, 43, low=20)
    col *= (1 - 0.25 * np.clip((pits - 0.62) * 6, 0, 1))[..., None]
    h = 0.6 * beds + 0.3 * fbm(n, 1.6, 44) - 0.2 * np.clip((pits - 0.62) * 6, 0, 1)
    return col, h


def scree(n):
    """Scree at an escarpment's foot: broken stones of every size on sand."""
    col = base(n, "B49A74", "A08662", 51) * tone(n, 52)
    h = 0.1 * fbm(n, 1.6, 53)
    for count, r, sd in ((int(n * n / 500), (2.0, 5.0), 54), (int(n * n / 3000), (6.0, 14.0), 55)):
        sc, sh, cv = stones(n, count, r, [hexc(p) for p in ("6E5A44", "8A7254", "9A8262", "B09A78", "C0AC88")], sd, squash=0.6)
        col, h = lay(col, h, sc, sh, cv)
    return col, h


def sebkha(n):
    """A salt flat: a pale grey-white crust in polygons, damp and darker where the ground water comes near."""
    cells = fbm(n, 1.0, 61, low=14)
    ridge = np.clip(1 - np.abs(cells - 0.5) * 22, 0, 1)
    col = base(n, "D8D0BC", "C2B6A0", 62, beta=2.2) * tone(n, 63, 0.06)
    col = col * (1 - 0.1 * ridge[..., None]) + 18 * ridge[..., None]
    h = 0.3 * ridge + 0.1 * fbm(n, 1.8, 64)
    return np.clip(col, 0, 255), h


def track(n):
    """A desert track: the stony crust broken by wheels and tracks, the paler grit under it showing in ruts."""
    col, h = gravel(n)
    x = np.mgrid[0:n, 0:n][1] / n
    w = fbm(n, 2.4, 71, low=1)
    rut = np.zeros((n, n))
    for c in (0.18, 0.40, 0.62, 0.84):
        d = np.abs(((x + 0.03 * (w - 0.5) - c + 0.5) % 1.0) - 0.5)
        rut = np.maximum(rut, np.clip(1 - d / 0.045, 0, 1))
    pale = base(n, "C6AE86", "B8A07A", 72) * tone(n, 73, 0.06)
    col = col * (1 - rut[..., None] * 0.85) + pale * rut[..., None] * 0.85
    return col, h * (1 - 0.8 * rut) - 0.3 * rut


# ---- the dry south ------------------------------------------------------------------------------------------------
def stubble(n):
    """Wheat stubble after the harvest (Sicily, July 1943): gold rows, straw lying between, earth showing."""
    rows = furrows(n, 28, 81, 0.02)
    col = base(n, "C4A86C", "B0925A", 82) * (0.9 + 0.14 * rows[..., None]) * tone(n, 83, 0.07)
    straw = np.clip((fbm(n, 0.6, 84, low=30) - 0.6) * 5, 0, 1)
    col = col * (1 - 0.5 * straw[..., None]) + hexc("DCC690") * 0.5 * straw[..., None]
    return col, 0.4 * rows + 0.1 * fbm(n, 1.6, 85)


def plough(n):
    """Ploughland: brown earth turned in furrows, clods catching the light."""
    rows = furrows(n, 20, 91, 0.04)
    col = base(n, "8A6E4E", "7A5E42", 92) * (0.82 + 0.26 * rows[..., None]) * tone(n, 93, 0.08)
    clods = fbm(n, 0.8, 94, low=24)
    return col * (0.92 + 0.16 * clods[..., None]), 0.7 * rows + 0.3 * clods


def fallow(n):
    """Fallow ground: dry grass gone ochre with green-brown patches of weeds, stones turned up."""
    col = base(n, "A89A62", "8E8A56", 101, lo=0.35, hi=0.65) * tone(n, 102, 0.1)
    grass = fbm(n, 0.7, 103, low=30)
    col *= (0.88 + 0.22 * grass[..., None])
    sc, sh, cv = stones(n, int(n * n / 5000), (1.6, 4.0), [hexc(p) for p in ("B4A484", "C8BCA0", "9A8E74")], 104)
    return lay(col, 0.3 * grass, sc, sh, cv)


def macchia_ground(n):
    """The dry hills under macchia: thin red-brown soil, pale limestone breaking through, dark scrub in clumps."""
    col = base(n, "8E7A58", "A08C64", 111) * tone(n, 112, 0.1)
    scrub = np.clip((fbm(n, 1.4, 113, low=6) - 0.55) * 5, 0, 1)
    col = col * (1 - scrub[..., None]) + (hexc("4A5432") * (0.85 + 0.3 * fbm(n, 0.6, 114, low=30)[..., None])) * scrub[..., None]
    sc, sh, cv = stones(n, int(n * n / 2600), (2.0, 6.0), [hexc(p) for p in ("B4A484", "C8BCA0", "A09070")], 115, squash=0.6)
    col, h = lay(col, 0.6 * scrub, sc, sh, cv * (1 - scrub))
    return col, h


def limestone(n):
    """Bare limestone of the southern hills: pale grey-buff, pitted and cracked, a little soil in the cracks."""
    pits = fbm(n, 0.9, 121, low=18)
    cracks = np.clip(1 - np.abs(fbm(n, 1.2, 122, low=6) - 0.5) * 26, 0, 1)
    col = base(n, "B4A484", "C4B698", 123) * tone(n, 124, 0.1)
    col = col * (1 - 0.3 * np.clip((pits - 0.6) * 5, 0, 1))[..., None]
    col = col * (1 - cracks[..., None]) + hexc("7A6A50") * cracks[..., None]
    return col, 0.6 * fbm(n, 1.4, 125) - 0.4 * cracks


SURFACES = {
    # name: (maker, metres the tile covers, colour seen from working height)
    "gravel": (gravel, 4.0), "sand": (sand, 6.0), "wadi_floor": (wadi_floor, 6.0), "escarp_rock": (escarp_rock, 8.0),
    "scree": (scree, 5.0), "sebkha": (sebkha, 8.0), "track": (track, 6.0),
    "stubble": (stubble, 5.0), "plough": (plough, 5.0), "fallow": (fallow, 6.0), "macchia_ground": (macchia_ground, 8.0),
    "limestone": (limestone, 8.0),
}


def make(name, out, n=512):
    fn, tile = SURFACES[name]
    col, h = fn(n)
    col = np.clip(col, 0, 255)
    h = h - h.min()
    h = h / max(h.max(), 1e-9)
    img = np.dstack([col, h * 255]).astype(np.uint8)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, name + ".png")
    Image.fromarray(img, "RGBA").save(path, optimize=True)
    seen = "".join(f"{int(round(c)):02X}" for c in col.reshape(-1, 3).mean(axis=0))
    return {"image": f"surfaces/{name}.png", "tile_m": tile, "colour": seen, "bytes": os.path.getsize(path)}


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "all" else ",".join(SURFACES)
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 512
    for name in which.split(","):
        info = make(name, out, n)
        print(name, info, flush=True)


if __name__ == "__main__":
    main()
