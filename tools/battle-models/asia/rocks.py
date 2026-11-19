"""Stone of the Asian hills, each of our own shape:

- RockOutcrop: the wet grey stone of Burma's and South China's hills, broken into flat faces, moss on its tops.
- DryOutcrop: the warm, bedded stone of the Zagros and Kurdish hills (164, 134, 104), laid in layers, no moss.
- Boulder, DryBoulder: one stone of each, a man's height, for scatter.

A stone is an icosphere pushed about by low noise and then cut by a handful of planes, so it shows the flat broken
faces of real rock rather than a smooth lump. Each face takes its paint from where it looks: tops weathered (moss on
the wet stone), sides the stone's own colour, undersides in shade.
"""
import numpy as np

from meshkit import TAU, Geo, icosphere

WET = {"stone": "7A7C78", "stone_dark": "5A5C5A", "stone_moss": "5E6A44", "stone_light": "9A9A92"}
DRY = {"stone": "A48668", "stone_dark": "7E6650", "stone_light": "BCA084", "stone_bed": "8C7058"}


def stone(g, rng, centre, radii, lod, wet=True, cuts=7, beds=0.0, tilt=0.0):
    """One broken stone. beds > 0 lays it in layers that thick (metres), the layers tipped by `tilt` radians."""
    V, F = icosphere((3, 2, 1)[lod])
    k = rng.normal(size=(6, 3))
    ph = rng.uniform(0, TAU, 6)
    bump = sum(np.sin(V @ k[i] * (1.2 + i * 0.6) + ph[i]) for i in range(6)) / 6.0
    V = V * (1.0 + 0.22 * bump)[:, None]
    # the breaks: flat cuts across the lump
    for _ in range(cuts if lod < 2 else max(3, cuts // 2)):
        n = rng.normal(size=3)
        n[2] = abs(n[2]) * 0.6 + (0.4 if rng.random() < 0.4 else -0.2)
        n /= np.linalg.norm(n)
        d = rng.uniform(0.55, 0.85)
        over = V @ n - d
        V = V - np.maximum(over, 0)[:, None] * n
    V = V * np.asarray(radii, float)
    if beds and lod < 2:
        # bedding: each layer's outer edge set back a little from the one below, so the stone reads as stacked slabs
        up = np.array((np.sin(tilt), 0.0, np.cos(tilt)))
        h = V @ up
        layer = np.floor(h / beds)
        frac = h / beds - layer
        shrink = 1.0 - 0.06 * np.clip((frac - 0.75) / 0.25, 0, 1) - 0.02 * (layer % 2)
        V[:, :2] *= shrink[:, None]
    V[:, 2] = np.maximum(V[:, 2], -radii[2] * 0.25)
    V = V + np.asarray(centre, float)
    V[:, 2] += -0.08 * max(radii[2], 0.3) - V[:, 2].min()          # sat in the ground, never floating
    base = g.add(V, [], "stone", 0.0)
    P = V[np.array(F)]
    c = P.mean(1)
    nrm = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    nz = nrm[:, 2] / (np.linalg.norm(nrm, axis=1) + 1e-9)
    # a smooth field over the stone decides its weathered patches, so they come in patches and not in specks
    kk = rng.normal(size=(4, 3)) * 1.4
    pp = rng.uniform(0, TAU, 4)
    field = sum(np.sin(c @ kk[i] + pp[i]) for i in range(4)) / 2.0
    zc = c[:, 2]
    for f, z, fl, cz in zip(F, nz, field, zc):
        if wet:
            if z > 0.5 and fl > -0.4:
                p = "stone_moss"
            elif z < -0.25:
                p = "stone_dark"
            elif fl > 0.55:
                p = "stone_light"
            elif fl < -0.7:
                p = "stone_dark"
            else:
                p = "stone"
        else:
            if z < -0.25:
                p = "stone_dark"
            elif z > 0.6 and fl > -0.2:
                p = "stone_light"
            elif beds and lod < 2 and int(cz / beds) % 3 == 1:
                p = "stone_bed"
            elif fl > 0.7:
                p = "stone_light"
            else:
                p = "stone"
        g.F.append([base + i for i in f])
        g.Fp.append(g.paint_names.index(p))
    return base


def outcrop(lod=0, seed=41, wet=True, size=6.0, name=None):
    """A spur of rock breaking through the hillside: one big stone, two or three leaning on it, a few loose ones."""
    rng = np.random.default_rng(seed)
    g = Geo(name or ("RockOutcrop" if wet else "DryOutcrop"), dict(WET if wet else DRY))
    s = size / 6.0
    beds = 0.0 if wet else 0.35 * s
    stone(g, rng, (0, 0, 0.6 * s), np.array((2.6, 1.8, 2.2)) * s, lod, wet, cuts=8, beds=beds, tilt=0.15)
    for k in range((3, 3, 2)[lod]):
        a = TAU * k / 3 + rng.uniform(-0.4, 0.4)
        d = rng.uniform(1.9, 2.6) * s
        c = (np.cos(a) * d, np.sin(a) * d * 0.8, 0.3 * s)
        stone(g, rng, c, np.array((rng.uniform(1.0, 1.5), rng.uniform(0.8, 1.2), rng.uniform(0.8, 1.4))) * s, lod, wet,
              cuts=6, beds=beds, tilt=0.15)
    if lod == 0:
        for k in range(5):
            a = rng.uniform(0, TAU)
            d = rng.uniform(3.0, 4.2) * s
            r = rng.uniform(0.2, 0.45) * s
            stone(g, rng, (np.cos(a) * d, np.sin(a) * d, 0.0), (r * 1.3, r, r * 0.8), 1, wet, cuts=4)
    g.info = {"title": ("Wet rock outcrop" if wet else "Dry bedded outcrop"), "high_m": round(3.0 * s, 1), "foot_m": round(4.0 * s, 1)}
    return g


def boulder(lod=0, seed=43, wet=True):
    rng = np.random.default_rng(seed)
    g = Geo("Boulder" if wet else "DryBoulder", dict(WET if wet else DRY))
    stone(g, rng, (0, 0, 0.45), (1.0, 0.8, 0.9), lod, wet, cuts=6, beds=0.0 if wet else 0.3)
    g.info = {"title": "Boulder" if wet else "Dry boulder", "high_m": 1.3, "foot_m": 1.1}
    return g


BUILDERS = {
    "RockOutcrop": lambda lod=0, seed=41: outcrop(lod, seed, wet=True),
    "DryOutcrop": lambda lod=0, seed=42: outcrop(lod, seed, wet=False),
    "Boulder": lambda lod=0, seed=43: boulder(lod, seed, wet=True),
    "DryBoulder": lambda lod=0, seed=44: boulder(lod, seed, wet=False),
}
