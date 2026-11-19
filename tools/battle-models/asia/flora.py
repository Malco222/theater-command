"""The plants of the Asian battlefields, each our own shape made from numbers, at three levels of detail.

Jungle (Burma, Malaya, the Indies): the tall emergent tree on its buttress roots, the canopy tree, the coconut palm,
the bamboo clump, the banana, the forest floor's broad leaves and ferns, elephant grass, rattan-hung lianas.
Rice country and the north (China): the hill pine with its flat crown, the willow by the ditch, the roadside poplar,
young rice, tall millet (gaoliang) and reeds.

A crown is built the way a painter blocks one in: a few lumpy masses of leaf (`leaf_shade`, the dark inside of the
crown, so no daylight shows through where a real crown is solid), each clothed on its outside with single leaves
that break the outline. Branches run from the trunk into the masses. Every builder takes lod 0, 1 or 2.
"""
import math

import numpy as np

from meshkit import TAU, Geo, bezier, leaf_cloud, perp, rot, unit

# Colours as seen, in the campaign map's palette (the battle grade goes over them in the game).
GREENS = {
    "leaf": "4A6636", "leaf_dark": "3A5430", "leaf_light": "64803E", "leaf_shade": "2C3E26", "leaf_dry": "8A8048",
}
BARK = {"bark": "6E6658", "bark_light": "9A9484", "bark_red": "7A5038", "bark_dark": "4A4038"}


def leafy(rng, k=3):
    """One of the leaf paints, mostly the middle green."""
    r = rng.random()
    return "leaf" if r < 0.5 else ("leaf_dark" if r < 0.8 else "leaf_light")


# ================================================================================================ crowns
LEAF_MIX = [("leaf", 0.42), ("leaf_dark", 0.33), ("leaf_light", 0.2), ("leaf_dry", 0.05)]


NEAR_LEAVES, NEAR_LEAF_SIZE = 0.45, 1.42


def child(rng):
    """A random stream of its own, for detail whose amount differs by LOD. The parent gives up one number for it at
    every LOD, so the parent's layout (limbs, masses, culms) is the same tree at near, mid and far."""
    return np.random.default_rng(int(rng.integers(1 << 30)))


def clump(g, rng, c, r, lod, n_leaves, leaf_len, leaf_w, sway=0.6, up_bias=0.25, shapes="oval", paints=None,
          droop=0.25, under=-0.55, needle=False, shade=None, mix=None):
    """A mass of leaf at c with radii r: a cloud of leaves filling its outer part round a small dark core (so no
    daylight shows through where a real crown is solid). Near: many small leaves; mid: fewer, larger; far: the core
    alone, lumped."""
    rng = child(rng)
    mix = [(p, s) for p, s in (mix or LEAF_MIX) if p in g.paint_hex]
    r = np.asarray(r, float)
    if lod == 2:
        g.blob(c, r, "leaf_shade", sub=1, sway=sway * 0.8, seed=int(rng.integers(1 << 30)), rough=0.3, flat_bottom=0.6,
               mottle=[(p, s) for p, s in [("leaf_shade", 0.4)] + mix[:2]])
        return
    g.blob(c, r * 0.55, "leaf_shade", sub=(2, 1)[lod], sway=sway * 0.8, seed=int(rng.integers(1 << 30)), rough=0.25, flat_bottom=0.5)
    vol = float(np.prod(r)) ** (2 / 3)                      # leaves go with the mass's surface
    # near: about half the leaves of a real crown, each a little larger, so a stand of trees keeps the frame rate
    n = int(n_leaves * vol / 4.0 * NEAR_LEAVES) if lod == 0 else int(n_leaves * vol / 4.0 / 5)
    k = NEAR_LEAF_SIZE if lod == 0 else 2.2
    if needle:
        leaf_cloud(g, rng, c, r, n, leaf_len * k, leaf_w * k * 1.5, mix, sway=(sway, min(1, sway + 0.3)), up_bias=up_bias + 0.3,
                   droop=0.05, shell=0.35, fold=0.0, tangent=0.6)
    else:
        leaf_cloud(g, rng, c, r, n, leaf_len * k, leaf_w * k, mix, sway=(sway, min(1, sway + 0.3)), up_bias=up_bias, droop=droop)


def limb(g, a, b, r0, r1, lod, paint="bark", sway=(0.1, 0.4), bend=0.25, rng=None, rough=None):
    """A branch from a to b, bowed upward a little."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    up = np.array((0, 0, 1.0)) * np.linalg.norm(d) * bend
    n = (9, 5, 3)[lod]
    P = bezier(a, a + d * 0.33 + up, a + d * 0.66 + up * 0.6, b, n)
    sides = (8, 5, 4)[lod]
    R = np.linspace(r0, r1, n)
    g.tube(P, R, sides, paint, sway=np.linspace(*sway, n), rough=rough, seed=0 if rng is None else int(rng.integers(1 << 20)))


def buttresses(g, rng, r_trunk, lod, n=6, reach=2.4, high=3.2, paint="bark_light"):
    """Thin flaring roots like fins round a rainforest giant's foot."""
    rng = child(rng)
    if lod == 2:
        return
    seg = (10, 5, 0)[lod]
    for k in range(n):
        a = TAU * k / n + rng.uniform(-0.25, 0.25)
        d = np.array((math.cos(a), math.sin(a), 0.0))
        s = np.array((-d[1], d[0], 0.0))
        R = reach * rng.uniform(0.7, 1.25)
        H = high * rng.uniform(0.7, 1.2)
        th = 0.16 * rng.uniform(0.8, 1.4) * max(1.0, r_trunk / 0.55)
        u = np.linspace(0, 1, seg + 1)
        rr = r_trunk * 0.7 + R * (1 - (1 - u) ** 2.2)
        zz = H * (1 - u) ** 1.8
        wig = R * 0.25 * np.sin(u * 3.0 + a * 3) * u ** 1.5 * (1 if k % 2 else -1)
        outer = rr[:, None] * d + wig[:, None] * s + np.stack([np.zeros_like(u), np.zeros_like(u), zz], 1)
        thick = th * (1 - u * 0.6)
        L0, L1 = outer + s * thick[:, None] * 0.5, outer - s * thick[:, None] * 0.5
        inner = np.column_stack([np.full(seg + 1, r_trunk * 0.7 * d[0]), np.full(seg + 1, r_trunk * 0.7 * d[1]), np.linspace(H, 0, seg + 1)])
        I0, I1 = inner + s * th * 0.6, inner - s * th * 0.6
        g.quad_strip(L0, I0, paint)                      # (each side faces out, the top edge faces up)
        g.quad_strip(I1, L1, paint)
        g.quad_strip(L1, L0, paint)


def crown(g, rng, stem, rt, spread, height, depth, n, lod, leaf_len, leaf_w, leaves=140, limb_paint="bark",
          r_mass=(1.3, 2.0), flat=0.0, sway=0.55, from_t=0.7, limbs=None):
    """A whole crown: a few main limbs leave the stem in its upper part and climb out to the crown; round each limb's
    end hang two to four masses of leaf, some lower and further in, so the limbs show only between them."""
    stem = np.asarray(stem, float)
    top = stem[-1]
    m = len(stem)
    k = limbs or max(3, n // 3)
    tips = []
    for i in range(k):
        a = TAU * (i + rng.uniform(-0.3, 0.3)) / k
        rr = spread * rng.uniform(0.45, 0.8)
        z = height * (1.0 - (1 - flat) * 0.5 * (rr / spread) ** 2) * rng.uniform(0.8, 1.0)
        end = top + np.array((math.cos(a) * rr, math.sin(a) * rr, z))
        t = from_t + (1 - from_t) * rng.uniform(0.0, 1.0)
        lr = child(rng)                                  # (this limb's detail; the parent keeps the layout)
        jf = t * (m - 1)                                 # (the same place on the stem whatever its LOD's sampling)
        j0 = min(m - 2, int(jf)) if m > 1 else 0
        f = min(1.0, jf - j0)
        s = stem[j0] * (1 - f) + stem[min(j0 + 1, m - 1)] * f
        rad = rt * (1 - 0.45 * t)
        q = s + (end - s) * 0.5 + np.array((0, 0, -0.05 * np.linalg.norm(end - s)))
        limb(g, s, q, rad * 0.6, rad * 0.35, lod, limb_paint, sway=(0.1, 0.3), bend=0.1, rng=lr, rough=(0.05, 3))
        limb(g, q, end, rad * 0.35, rad * 0.12, lod, limb_paint, sway=(0.3, 0.5), bend=0.15, rng=lr)
        per = int(round(n / k))
        for q_ in range(per):
            off = np.array((*lr.normal(size=2) * spread * 0.22, lr.normal() * depth * 0.4 - (0.0 if q_ == 0 else depth * 0.35)))
            p = end + off
            # pull outer masses in a little so the crown keeps its outline
            d2 = np.linalg.norm(p[:2] - top[:2])
            if d2 > spread:
                p[:2] = top[:2] + (p[:2] - top[:2]) * spread / d2
            R = lr.uniform(*r_mass)
            clump(g, lr, p, np.array((R * lr.uniform(1.0, 1.3), R * lr.uniform(1.0, 1.3), R * lr.uniform(0.7, 0.9))),
                  lod, leaves, leaf_len, leaf_w, sway=sway)
            tips.append(p)
    return tips


def climber(g, rng, stem, R, lod, turns=2.5, from_t=0.05, to_t=0.75, paint="bark_dark", leaves=True):
    """A woody climber wound round the trunk, with a leaf here and there: the jungle's lianas, kept on the tree."""
    rng = child(rng)
    if lod == 2:
        return
    stem = np.asarray(stem, float)
    m = len(stem)
    n = (60, 20)[lod]
    ph = rng.uniform(0, TAU)
    pts = []
    for i in range(n):
        t = from_t + (to_t - from_t) * i / (n - 1)
        j = t * (m - 1)
        j0 = int(j)
        f = j - j0
        c = stem[j0] * (1 - f) + stem[min(j0 + 1, m - 1)] * f
        r = (R[j0] * (1 - f) + R[min(j0 + 1, m - 1)] * f) * 1.08 + 0.04
        a = ph + TAU * turns * i / (n - 1)
        pts.append(c + np.array((math.cos(a) * r, math.sin(a) * r, 0.0)))
    pts = np.array(pts)
    g.tube(pts, np.linspace(0.07, 0.035, n), (6, 4)[lod], paint, sway=np.linspace(0.0, 0.2, n), cap1=True)
    if leaves:
        for i in range(4, n, (3, 6)[lod]):
            d = unit(pts[i] - pts[i - 1] + rng.normal(size=3) * 0.6 + np.array((0, 0, -0.2)))
            g.blade(pts[i], d, unit(np.cross(d, (0, 0, 1))), rng.uniform(0.18, 0.3), 0.1, 1, leafy(rng), droop=0.6, fold=0.3, sway0=0.3, sway1=0.6, shape="oval")


# ================================================================================================ jungle
def jungle_giant(lod=0, seed=1, H=34.0):
    """The emergent tree that stands over the canopy: a pale straight bole on buttress roots, its great limbs breaking
    out high up into a broad, flat-topped crown; a climber wound up its trunk."""
    rng = np.random.default_rng(seed)
    g = Geo("JungleGiant", {**GREENS, **BARK})
    rt = 0.85 * H / 34.0
    lean = rng.normal(size=2) * 0.4
    top = np.array((lean[0], lean[1], H * 0.7))
    P = bezier(np.zeros(3), np.array((0, 0, H * 0.25)), top * 0.7 + np.array((0, 0, H * 0.1)), top, (24, 10, 5)[lod])
    zz = P[:, 2]
    R = rt * (1.0 - 0.45 * np.linspace(0, 1, len(P)) ** 1.2) * (1.0 + 0.7 * np.exp(-zz / (2.5 * H / 34)))
    g.tube(P, R, (16, 9, 5)[lod], "bark_light", sway=np.linspace(0, 0.12, len(P)), rough=(0.06, 5), seed=seed)
    buttresses(g, rng, rt, lod, n=int(rng.integers(7, 10)), reach=3.2 * H / 34, high=4.6 * H / 34)
    crown(g, rng, P, rt, H * 0.3, H * 0.2, H * 0.06, (18, 13, 10)[lod], lod, 0.55, 0.22, leaves=600,
          limb_paint="bark_light", r_mass=(2.2 * H / 34, 3.2 * H / 34), flat=0.75, from_t=0.72, limbs=6)
    climber(g, rng, P, R, lod, turns=3.0, to_t=0.85)
    g.info = {"title": "Jungle giant", "high_m": round(H * 0.98, 1), "foot_m": round(rt + 3.0 * H / 34, 1)}
    return g


def jungle_tree(lod=0, seed=2, H=18.0):
    """The canopy tree: a leaning trunk forking into spreading limbs and a deep rounded crown of many masses."""
    rng = np.random.default_rng(seed)
    g = Geo("JungleTree", {**GREENS, **BARK})
    rt = 0.36 * H / 18
    fork = H * rng.uniform(0.45, 0.55)
    lean = np.array((*rng.normal(size=2) * 0.8, 0.0))
    top = lean + np.array((0, 0, fork))
    P = bezier(np.zeros(3), np.array((0, 0, fork * 0.4)), top - lean * 0.3, top, (16, 8, 4)[lod])
    R = rt * np.linspace(1.15, 0.8, len(P))
    g.tube(P, R, (12, 7, 5)[lod], "bark", sway=np.linspace(0, 0.1, len(P)), rough=(0.05, 4), seed=seed)
    buttresses(g, rng, rt, lod, n=4, reach=1.0 * H / 18, high=1.3 * H / 18, paint="bark")
    crown(g, rng, P, rt, H * 0.26, H * 0.34, H * 0.16, (15, 11, 8)[lod], lod, 0.36, 0.16, leaves=650,
          r_mass=(1.7 * H / 18, 2.4 * H / 18), from_t=0.6, limbs=5)
    if rng.random() < 0.5:
        climber(g, rng, P, R, lod, turns=1.5, to_t=0.95)
    g.info = {"title": "Jungle canopy tree", "high_m": round(H, 1), "foot_m": round(rt + 1.0, 1)}
    return g


def coconut_palm(lod=0, seed=3, H=18.0, lean=0.18, name="CoconutPalm", title="Coconut palm"):
    """A coconut palm: a slim grey trunk ringed with old leaf scars, swelling at the foot and bowing as it rises;
    a head of long arching fronds, each hung with two rows of narrow folded leaflets; a cluster of nuts."""
    rng = np.random.default_rng(seed)
    g = Geo(name, {"bark": "7C7262", "bark_dark": "564E44", "frond": "5E7A3A", "frond_light": "7A9446",
                            "frond_dry": "8C7A50", "nut": "6A6232"})
    a = rng.uniform(0, TAU)
    d = np.array((math.cos(a), math.sin(a), 0.0))
    topc = d * H * lean + np.array((0, 0, H))
    n = (int(H / 0.16), 22, 6)[lod]
    P = bezier(np.zeros(3), np.array((0, 0, H * 0.35)) + d * H * lean * 0.05, topc - d * H * lean * 0.35 - np.array((0, 0, H * 0.25)), topc, n)
    u = np.linspace(0, 1, n)
    R = 0.2 + 0.16 * np.exp(-u * H / 1.2) - 0.03 * u
    if lod == 0:
        R = R * (1 + 0.035 * np.sign(np.sin(u * H / 0.16 * math.pi)))       # the rings of old leaf scars
    g.tube(P, R, (12, 7, 5)[lod], "bark", sway=np.clip(u ** 2 * 0.35, 0, 1), cap0=False)
    T = unit(P[-1] - P[-2])
    # the crown shaft and boots of the fronds
    if lod < 2:
        g.lathe([(0.24, -0.5), (0.3, -0.1), (0.26, 0.25), (0.1, 0.45)], (10, 6, 4)[lod], "bark_dark", at=topc, sway=0.35)
    # nuts
    nr = child(rng)
    if lod < 2:
        for k in range((9, 4, 0)[lod]):
            b = nr.uniform(0, TAU)
            c = topc + np.array((math.cos(b) * 0.32, math.sin(b) * 0.32, -0.25 + nr.uniform(-0.15, 0.1)))
            g.blob(c, (0.12, 0.12, 0.14), "nut", sub=1, sway=0.35, seed=k + seed * 7, rough=0.08)
    nf = (22, 14, 10)[lod]
    for k in range(nf):
        b = TAU * k / nf + rng.uniform(-0.2, 0.2)
        out = np.array((math.cos(b), math.sin(b), 0.0))
        age = k / nf
        rise = rng.uniform(0.9, 1.6) if age < 0.3 else (rng.uniform(0.3, 0.9) if age < 0.75 else rng.uniform(-0.5, 0.1))   # young fronds stand up, old ones hang
        dry = age > 0.88 and lod < 2
        L = rng.uniform(4.2, 5.4) * (H / 18) ** 0.35
        base = topc + np.array((0, 0, 0.3)) + out * 0.15
        p1 = base + out * L * 0.35 + np.array((0, 0, L * 0.35 * rise))
        p2 = base + out * L * 0.75 + np.array((0, 0, L * (0.25 * rise - 0.15)))
        p3 = base + out * L * 0.95 + np.array((0, 0, -L * (0.45 - 0.25 * rise)))
        m = (14, 8, 5)[lod]
        rach = bezier(base, p1, p2, p3, m)
        g.tube(rach, np.linspace(0.05, 0.012, m), (5, 3, 3)[lod], "frond_dry" if dry else "frond", sway=np.linspace(0.4, 1.0, m), cap1=False)
        if lod == 2:
            # one broad bent strip: reads as a frond from far off
            side = unit(np.cross(out, (0, 0, 1)))
            Lf = np.concatenate([rach + side * w for w in [0.0]])
            wid = 0.55 * np.sin(np.linspace(0.15, 1.0, m) * math.pi) ** 0.6
            g.quad_strip(rach + side[None] * wid[:, None] - np.array((0, 0, 0.25)) * wid[:, None],
                         rach - side[None] * wid[:, None] - np.array((0, 0, 0.25)) * wid[:, None], "frond", np.linspace(0.4, 1, m), np.linspace(0.4, 1, m))
            g.two_sided.add("frond")
            continue
        T_r = np.gradient(rach, axis=0)
        pairs = (30, 14, 0)[lod]
        for j in range(pairs):
            t = 0.1 + 0.88 * j / pairs
            i0 = t * (m - 1)
            i = int(i0)
            f = i0 - i
            p = rach[i] * (1 - f) + rach[min(i + 1, m - 1)] * f
            tg = unit(T_r[i])
            side = unit(np.cross(tg, (0, 0, 1)))
            ll = 0.95 * math.sin(math.pi * (0.15 + 0.8 * t)) ** 0.7 * (H / 18) ** 0.2
            for s in (1, -1):
                dirn = unit(side * s * 0.75 + tg * 0.55 + np.array((0, 0, -0.35 - 0.3 * t)))
                sd = unit(np.cross(dirn, (0, 0, 1)))
                paint = "frond_dry" if dry else ("frond_light" if rng.random() < 0.3 else "frond")
                w = 0.045 if lod == 0 else 0.075
                g.blade(p, dirn, sd, ll * rng.uniform(0.85, 1.1), w, (2, 1)[lod], paint, droop=0.6 + 0.4 * t, fold=0.5,
                        sway0=0.45 + 0.5 * t, sway1=min(1.0, 0.6 + 0.5 * t), shape="lance")
    g.info = {"title": title, "high_m": round(H + 1.5, 1), "foot_m": 0.4}
    return g


def bamboo(lod=0, seed=4, H=12.0):
    """A clump of bamboo: thirty-odd culms from one root mass, the outer ones arching far out, each jointed at its
    nodes, their upper two thirds feathered thick with sprays of narrow leaves, so the clump is a green fountain."""
    rng = np.random.default_rng(seed)
    g = Geo("Bamboo", {"bamboo": "7E8A4A", "bamboo_old": "9C9460", "leaf": "5E7A3A", "leaf_dark": "4A6432",
                       "leaf_light": "7A8E46", "leaf_dry": "9A9058", "leaf_shade": "34482A", "earth": "4A4232"})
    nc = (30, 18, 10)[lod]
    g.blob((0, 0, 0.0), (1.1, 1.1, 0.3), "earth", sub=1, sway=0.0, seed=seed, rough=0.2)
    for k in range(nc):
        a = rng.uniform(0, TAU)
        rr = math.sqrt(rng.uniform(0, 1)) * 0.9
        foot = np.array((math.cos(a) * rr, math.sin(a) * rr, 0.0))
        out = np.array((math.cos(a), math.sin(a), 0.0))
        h = H * rng.uniform(0.75, 1.1)
        arch = (0.15 + 0.85 * rr ** 1.5) * rng.uniform(0.7, 1.15)
        tip = foot + out * h * arch * 0.75 + np.array((0, 0, h * (1 - 0.42 * arch)))
        n = (int(h / 0.3), 10, 4)[lod]
        P = bezier(foot, foot + np.array((0, 0, h * 0.45)), foot + out * h * arch * 0.3 + np.array((0, 0, h * (1 - 0.2 * arch))), tip, n)
        u = np.linspace(0, 1, n)
        R = rng.uniform(0.035, 0.06) * (1 - 0.75 * u)
        if lod == 0:
            R = R * np.where(np.arange(n) % 2 == 0, 1.1, 1.0)          # the nodes swell
        g.tube(P, R, (7, 5, 3)[lod], "bamboo_old" if rng.random() < 0.3 else "bamboo", sway=u ** 1.5, cap1=False)
        lr = child(rng)                                  # (the culm's leaves: their own stream)
        if lod == 2:
            g.blob(P[int(n * 0.75)], (0.8, 0.8, h * 0.18), "leaf_shade", sub=1, sway=0.8, seed=k, rough=0.35, mottle=[("leaf_shade", 0.6), ("leaf", 0.4)])
            continue
        steps = (8, 4)[lod]
        for q, t in enumerate(np.linspace(0.38, 0.98, steps)):
            i = min(n - 1, int(t * (n - 1)))
            rr_ = lr.uniform(0.5, 0.75) * (0.7 + 0.5 * t)
            tg = unit(P[min(n - 1, i + 1)] - P[max(0, i - 1)])
            c = P[i] + lr.normal(size=3) * 0.12
            leaf_cloud(g, lr, c, (rr_, rr_, rr_ * 0.9), (34, 22)[lod], 0.26 * (1.6 if lod else 1.18), 0.042 * (1.6 if lod else 1.18),
                       [("leaf", 0.45), ("leaf_dark", 0.25), ("leaf_light", 0.25), ("leaf_dry", 0.05)],
                       sway=(min(1, t), 1.0), up_bias=-0.2, droop=0.7, shell=1.0, fold=0.1, tangent=1.0)
    g.info = {"title": "Bamboo clump", "high_m": round(H * 1.05, 1), "foot_m": 1.2}
    return g


def torn_leaf(g, rng, base, d, L, W, lod, paint, droop=1.0, tears=9, sway=(0.45, 1.0)):
    """A banana leaf: a long oblong blade on a stiff midrib that arches out and down, its two halves torn by the wind
    from the edge in to the midrib, so each half hangs as a comb of strips."""
    side = unit(np.cross(d, (0, 0, 1)))
    up = unit(np.cross(side, d))
    n = (max(8, tears * 2), 6, 3)[lod]
    u = np.linspace(0, 1, n + 1)
    mid = np.array([base + rot(side, -droop * t * t) @ d * L * t for t in u])          # midrib curve
    T = np.gradient(mid, axis=0)
    w = W * np.sin(np.pi * np.clip(0.06 + u * 0.94, 0, 1)) ** 0.45
    g.tube(mid, np.linspace(0.02, 0.006, n + 1), (4, 3, 3)[lod], "stem", sway=np.linspace(*sway, n + 1), cap1=False)
    g.two_sided.add(paint)
    for s in (1, -1):
        for i in range(n):
            if lod == 0 and tears and rng.random() < 0.18:
                continue                                            # a strip torn away
            gap = 0.12 if (lod == 0 and tears) else 0.0
            ta, tb = u[i], u[i + 1]
            ea = ta + (tb - ta) * gap
            eb = tb - (tb - ta) * gap
            sa = unit(np.cross(unit(T[i]), up)) * s
            sb = unit(np.cross(unit(T[i + 1]), up)) * s
            hang = 0.35 + 0.4 * rng.random()                        # the strip's edge hangs below the midrib
            p0, p1 = mid[i], mid[i + 1]
            e0 = mid[i] + (mid[i + 1] - mid[i]) * gap + (sa - up * hang) * w[i] / math.hypot(1, hang)
            e1 = mid[i + 1] - (mid[i + 1] - mid[i]) * gap + (sb - up * hang) * w[i + 1] / math.hypot(1, hang)
            sw0, sw1 = sway[0] + (sway[1] - sway[0]) * ta, sway[0] + (sway[1] - sway[0]) * tb
            g.add([p0, p1, e1, e0], [[0, 1, 2, 3]], paint, [sw0, sw1, min(1, sw1 + 0.1), min(1, sw0 + 0.1)])


def banana(lod=0, seed=5, H=3.6):
    """A banana plant: a soft trunk of wrapped leaf sheaths and a fountain of huge leaves, torn by the wind into strips."""
    rng = np.random.default_rng(seed)
    g = Geo("Banana", {"stem": "6A7A42", "stem_dark": "5A5636", "leaf": "5C7E38", "leaf_light": "7A9A44", "leaf_dry": "8A7448"})
    n = (10, 5, 3)[lod]
    P = np.column_stack([np.zeros(n), np.zeros(n), np.linspace(0, H * 0.6, n)])
    g.tube(P, np.linspace(0.17, 0.11, n), (12, 6, 4)[lod], "stem", sway=np.linspace(0, 0.3, n), rough=(0.05, 3))
    nl = (9, 7, 5)[lod]
    top = P[-1]
    for k in range(nl):
        b = TAU * k * 0.382 + rng.uniform(-0.2, 0.2)
        out = np.array((math.cos(b), math.sin(b), 0.0))
        dry = k >= nl - 2
        young = k < 2
        ang = (1.25 if young else rng.uniform(0.55, 0.95)) if not dry else -0.6
        L = rng.uniform(1.7, 2.3) * (0.7 if young else 1.0)
        stalk = top + np.array((0, 0, rng.uniform(-0.15, 0.25)))
        d = unit(out * math.cos(ang) + np.array((0, 0, math.sin(ang))))
        mid = stalk + d * 0.45
        g.tube(np.array([stalk, mid]), [0.045, 0.03], (5, 4, 3)[lod], "stem", sway=[0.3, 0.45], cap1=False)
        pnt = "leaf_dry" if dry else ("leaf_light" if (young or rng.random() < 0.25) else "leaf")
        torn_leaf(g, rng, mid, d, L, 0.32, lod, pnt, droop=(0.5 if young else 1.4) if not dry else 0.6, tears=0 if young else 9)
    g.info = {"title": "Banana plant", "high_m": round(H, 1), "foot_m": 0.3}
    return g


def undergrowth(lod=0, seed=6):
    """The forest floor's thicket: broad heart-shaped leaves on long stalks and two or three ferns."""
    rng = np.random.default_rng(seed)
    g = Geo("Undergrowth", {"stem": "5C6A3A", "leaf": "40602E", "leaf_dark": "34502A", "leaf_light": "5E7E3A", "frond": "4E6E34"})
    for k in range((22, 9, 5)[lod]):
        a = rng.uniform(0, TAU)
        out = np.array((math.cos(a), math.sin(a), 0.0))
        foot = out * rng.uniform(0, 0.6)
        h = rng.uniform(0.5, 1.2)
        tip = foot + out * rng.uniform(0.2, 0.5) + np.array((0, 0, h))
        g.tube(bezier(foot, foot + np.array((0, 0, h * 0.5)), tip - out * 0.1, tip, (5, 3, 2)[lod]), np.linspace(0.015, 0.008, (5, 3, 2)[lod]),
               (4, 3, 3)[lod], "stem", sway=np.linspace(0, 0.6, (5, 3, 2)[lod]), cap1=False)
        d = unit(out * 0.8 + np.array((0, 0, rng.uniform(-0.3, 0.2))))
        g.blade(tip, d, unit(np.cross(d, (0, 0, 1))), rng.uniform(0.45, 0.8), rng.uniform(0.22, 0.32), (3, 2, 1)[lod],
                ("leaf", "leaf_dark", "leaf_light")[int(rng.integers(3))], droop=0.7, fold=0.25, sway0=0.6, sway1=1.0, shape="oval")
    if lod < 2:
        for f in range(3):
            c = rng.normal(size=3) * np.array((0.5, 0.5, 0)) + np.array((0, 0, 0.02))
            for k in range((7, 4)[lod]):
                a = TAU * k / (7, 4)[lod] + rng.uniform(-0.2, 0.2)
                out = np.array((math.cos(a), math.sin(a), 0.0))
                L = rng.uniform(0.8, 1.2)
                m = (8, 4)[lod]
                rach = bezier(c, c + out * L * 0.2 + np.array((0, 0, L * 0.5)), c + out * L * 0.7 + np.array((0, 0, L * 0.45)), c + out * L + np.array((0, 0, 0.1)), m)
                for j in range(1, m):
                    sd = unit(np.cross(rach[j] - rach[j - 1], (0, 0, 1)))
                    for s in (1, -1):
                        ln = 0.18 * math.sin(math.pi * j / m) + 0.04
                        g.blade(rach[j], unit(sd * s + out * 0.4), unit(rach[j] - rach[j - 1]), ln, 0.035 if lod == 0 else 0.07, 1, "frond",
                                droop=0.3, sway0=0.5, sway1=1.0, shape="lance")
    g.info = {"title": "Forest floor thicket", "high_m": 1.3, "foot_m": 0.9}
    return g


def tall_grass(lod=0, seed=7, H=2.8, name="ElephantGrass", plumes=10, wide=0.04, n=150, title="Elephant grass",
               paints=None, plume_paint="seed"):
    """A clump of tall grass (Burma's elephant grass; the reeds of a marsh): long arching blades, green below and
    straw at the tips, and a few stalks with feathery heads standing out of it."""
    rng = np.random.default_rng(seed)
    g = Geo(name, paints or {"grass": "6E7A40", "grass_light": "8A9050", "grass_dry": "A49A62", "seed": "C4B892", "straw": "9A8E5E"})
    nb = (n, n // 3, n // 8)[lod]
    for k in range(nb):
        a = rng.uniform(0, TAU)
        out = np.array((math.cos(a), math.sin(a), 0.0))
        rr = rng.uniform(0, 1)
        foot = out * rr * 0.45
        L = H * rng.uniform(0.6, 1.05)
        d = unit(out * (0.1 + 0.6 * rr) + np.array((0, 0, 1.0)))
        side = unit(np.cross(d, out) + rng.normal(size=3) * 0.2)
        pnt = list(g.paint_hex)[int(rng.choice(3, p=(0.55, 0.3, 0.15)))]
        g.blade(foot, d, side, L, wide * (2.2 if lod == 2 else (1.5 if lod else 1)), (5, 3, 2)[lod], pnt, droop=rng.uniform(0.9, 2.2) * (0.5 + rr),
                fold=0.3, sway0=0.0, sway1=1.0, shape="grass")
    for k in range((plumes, plumes // 2, 0)[lod]):
        a = rng.uniform(0, TAU)
        out = np.array((math.cos(a), math.sin(a), 0.0))
        h = H * rng.uniform(1.05, 1.3)
        tip = out * h * 0.15 + np.array((0, 0, h))
        P = bezier(np.zeros(3), np.array((0, 0, h * 0.5)), tip - np.array((0, 0, 0.3)), tip, (6, 3)[lod])
        g.tube(P, np.linspace(0.008, 0.004, len(P)), 3, "straw", sway=np.linspace(0, 1, len(P)), cap1=False)
        for q in range((10, 4)[lod]):
            dd = unit(out * 0.3 + np.array((0, 0, 0.4)) + rng.normal(size=3) * 0.4)
            g.blade(tip - np.array((0, 0, 0.35 * q / 10)), dd, unit(np.cross(dd, (0, 0, 1))), rng.uniform(0.18, 0.32), 0.02, 1, plume_paint,
                    droop=0.5, sway0=1.0, sway1=1.0, shape="grass")
    g.info = {"title": title, "high_m": round(H * 1.25, 1), "foot_m": 0.6}
    return g


# ================================================================================================ China
def hill_pine(lod=0, seed=8, H=13.0):
    """The pine of China's and Burma's hills: a crooked, reddish trunk bare to two thirds of its height, and flat,
    layered pads of needles held out on horizontal limbs."""
    rng = np.random.default_rng(seed)
    g = Geo("HillPine", {"bark_red": "7A5038", "bark_dark": "4E3A2E", "needle": "3E5432", "needle_light": "56683A", "leaf_shade": "2A3826"})
    n = (14, 7, 4)[lod]
    pts = [np.zeros(3)]
    d = np.array((0, 0, 1.0))
    for i in range(1, 14):                               # (the trunk's line drawn once, sampled per LOD)
        d = unit(d + np.array((*rng.normal(size=2) * 0.07, 0.0)))
        pts.append(pts[-1] + d * H * 0.92 / 13)
    P14 = np.array(pts)
    s14 = np.linspace(0, 1, 14)
    sn = np.linspace(0, 1, n)
    P = np.column_stack([np.interp(sn, s14, P14[:, c]) for c in range(3)])
    u = np.linspace(0, 1, n)
    g.tube(P, 0.3 * (1 - 0.75 * u) * H / 13, (10, 6, 4)[lod], "bark_red", sway=u * 0.3, rough=(0.08, 3), seed=seed)
    # the nine pads are laid out once; the mid and far trees keep a spread of them (the lowest and widest always)
    lay = []
    for k in range(9):
        t = 0.55 + 0.45 * k / 9
        a = rng.uniform(0, TAU) if k else 0
        reach = (1.0 - (t - 0.55) * 1.3) * H * 0.26 * rng.uniform(0.7, 1.1) + 0.6
        lay.append((t, a, reach, rng.uniform(0.0, 0.6), 0.75 + 0.25 * rng.random(), child(rng)))
    keep = sorted(set(np.round(np.linspace(0, 8, (9, 7, 5)[lod])).astype(int)))
    for k in keep:
        t, a, reach, lift, rz, lr = lay[k]
        i = min(n - 1, int(round(t * (n - 1))))
        base = P14[min(13, int(round(t * 13)))]          # (on the trunk's own line, whatever the LOD)
        out = np.array((math.cos(a), math.sin(a), 0.0))
        tip = base + out * reach + np.array((0, 0, lift))
        limb(g, base, tip, 0.07 * H / 13, 0.025, lod, "bark_dark", sway=(u[i] * 0.3, 0.6), bend=0.05, rng=lr)
        r = np.array((reach * 0.55 + 0.9, reach * 0.55 + 0.9, rz))
        clump(g, lr, tip + np.array((0, 0, 0.25)), r, lod, 2600, 0.3, 0.06, sway=0.6, up_bias=0.6, needle=True,
              mix=[("needle", 0.65), ("needle_light", 0.35)])
    clump(g, rng, P[-1] + np.array((0, 0, 0.3)), np.array((1.6, 1.6, 0.7)), lod, 2600, 0.3, 0.06, sway=0.6, up_bias=0.6, needle=True,
          mix=[("needle", 0.65), ("needle_light", 0.35)])
    g.info = {"title": "Hill pine", "high_m": round(H + 0.8, 1), "foot_m": 0.3}
    return g


def willow(lod=0, seed=9, H=9.0):
    """The willow of every Chinese ditch and pond: a short thick trunk cut back again and again (a knobbed head), a
    ring of upswept limbs, a rounded head and long hanging curtains of leaf."""
    rng = np.random.default_rng(seed)
    g = Geo("Willow", {"bark": "5E564A", "bark_dark": "48403A", "leaf": "6A7E44", "leaf_light": "8A9A56", "leaf_shade": "3C4C2C"})
    s = H / 9
    top = np.array((*rng.normal(size=2) * 0.25, 2.6 * s))
    nt = (8, 5, 3)[lod]
    g.tube(bezier(np.zeros(3), np.array((0, 0, 1.0 * s)), top * 0.8, top, nt), np.linspace(0.42, 0.34, nt) * s,
           (12, 7, 4)[lod], "bark", sway=np.linspace(0, 0.05, nt), rough=(0.16, 4), seed=seed)
    if lod < 2:
        g.blob(top + np.array((0, 0, 0.05 * s)), np.array((0.42, 0.42, 0.28)) * s, "bark", sub=(2, 1)[lod], seed=seed, rough=0.3)
    nl = (7, 6, 5)[lod]
    for k in range(nl):
        a = TAU * k / nl + rng.uniform(-0.25, 0.25)
        rad = np.array((math.cos(a), math.sin(a), 0.0))
        reach = rng.uniform(1.6, 2.4) * s
        tip = top + rad * reach + np.array((0, 0, rng.uniform(2.6, 4.0) * s))
        limb(g, top + np.array((0, 0, 0.15 * s)), tip, 0.12 * s, 0.04, lod, "bark_dark", sway=(0.05, 0.35), bend=0.2, rng=rng)
        cr = np.array((1.5, 1.5, 1.3)) * s
        clump(g, rng, tip, cr, lod, 1900, 0.3, 0.07, sway=0.6, droop=0.9, mix=[("leaf", 0.5), ("leaf_light", 0.4), ("leaf_shade", 0.1)])
        if lod < 2:
            # the hanging curtains: long narrow clouds of leaf from the crown's outer lower edge, leaves pointing down
            for q in range((3, 2)[lod]):
                side = np.array((-rad[1], rad[0], 0.0)) * rng.uniform(-0.8, 0.8) * s
                hang = rng.uniform(1.0, 1.6) * s
                b = tip + rad * 1.0 * s + side - np.array((0, 0, 0.6 * s + hang))
                leaf_cloud(g, rng, b, np.array((0.4 * s, 0.4 * s, hang)), (150, 40)[lod], 0.24 * (1.6 if lod else 1),
                           0.05 * (1.6 if lod else 1), [("leaf", 0.5), ("leaf_light", 0.5)], sway=(0.7, 1.0), up_bias=0.0,
                           droop=1.6, shell=1.0, fold=0.1, tangent=0.3)
    g.info = {"title": "Willow", "high_m": round(H, 1), "foot_m": 0.45}
    return g


def poplar(lod=0, seed=10, H=16.0):
    """The tall narrow poplar planted in rows along North China's and Manchuria's roads."""
    rng = np.random.default_rng(seed)
    g = Geo("Poplar", {"bark_light": "8C8A7A", "bark": "5E5A4E", "leaf": "56703A", "leaf_light": "7A8E4C", "leaf_shade": "34462A"})
    n = (12, 6, 3)[lod]
    P = np.column_stack([np.zeros(n), np.zeros(n), np.linspace(0, H * 0.95, n)])
    g.tube(P, np.linspace(0.22, 0.05, n) * H / 16, (9, 6, 4)[lod], "bark_light", sway=np.linspace(0, 0.4, n))
    for k in range((5, 4, 3)[lod]):
        t = 0.3 + 0.55 * k / 4
        c = np.array((*rng.normal(size=2) * 0.2, H * t))
        r = np.array((1.25, 1.25, H * 0.2)) * (1.0 - 0.35 * abs(t - 0.5))
        clump(g, rng, c, r, lod, 4000, 0.15, 0.10, sway=0.4 + 0.5 * t, up_bias=0.6, droop=0.1)
    g.info = {"title": "Poplar", "high_m": round(H, 1), "foot_m": 0.25}
    return g


def rice_patch(lod=0, seed=11, size=2.0):
    """Young rice standing in a flooded paddy: tufts in rows a quarter of a metre apart (a 2 m square, tiled)."""
    rng = np.random.default_rng(seed)
    g = Geo("RicePatch", {"grass": "5E8238", "grass_light": "7C9A44", "grass_dry": "8E9A4C"})
    step = 0.25 if lod == 0 else 0.5
    k = int(size / step)
    for i in range(k):
        for j in range(k):
            c = np.array(((i + 0.5) * step - size / 2, (j + 0.5) * step - size / 2, 0.0)) + np.array((*rng.normal(size=2) * 0.02, 0))
            for q in range((7, 4, 0)[lod]):
                a = rng.uniform(0, TAU)
                out = np.array((math.cos(a), math.sin(a), 0.0))
                d = unit(out * rng.uniform(0.1, 0.4) + np.array((0, 0, 1)))
                g.blade(c, d, unit(np.cross(d, out)), rng.uniform(0.45, 0.75), 0.012 * (1 if lod == 0 else 2.2), (2, 1)[lod],
                        list(g.paint_hex)[int(rng.integers(3))], droop=rng.uniform(0.3, 0.9), sway0=0.0, sway1=1.0, shape="grass")
    if lod == 2:
        g.add([(-size / 2, -size / 2, 0.45), (size / 2, -size / 2, 0.45), (size / 2, size / 2, 0.45), (-size / 2, size / 2, 0.45)], [[0, 1, 2, 3]], "grass", 0.5)
    g.info = {"title": "Young rice (2 m square)", "high_m": 0.8, "foot_m": size / 2}
    return g


def gaoliang(lod=0, seed=12, size=3.0):
    """Tall millet (gaoliang), North China's summer crop: stalks taller than a man in rows, strap leaves and a
    reddish seed head on each, so thick that whole armies hid in it."""
    rng = np.random.default_rng(seed)
    g = Geo("Gaoliang", {"stem": "7A7C44", "leaf": "62743C", "leaf_light": "86904E", "leaf_dry": "A49660", "grain": "6A3C28"})
    rows, per = (6, 4, 2)[lod], (11, 5, 2)[lod]
    for i in range(rows):
        for j in range(per):
            foot = np.array(((i + 0.5) * size / rows - size / 2, (j + 0.5) * size / per - size / 2, 0.0)) + np.array((*rng.normal(size=2) * 0.06, 0))
            h = rng.uniform(2.4, 3.2)
            lean = np.array((*rng.normal(size=2) * 0.08, 0))
            n = (8, 4, 2)[lod]
            P = bezier(foot, foot + np.array((0, 0, h * 0.4)), foot + lean * h * 0.5 + np.array((0, 0, h * 0.8)), foot + lean * h + np.array((0, 0, h)), n)
            g.tube(P, np.linspace(0.016, 0.01, n), (5, 4, 3)[lod], "stem", sway=np.linspace(0, 0.9, n), cap1=False)
            head = P[-1] + np.array((0, 0, 0.18)) + lean * 0.4
            g.tube(np.array([P[-1], head]), 0.007, 3, "stem", sway=1.0, cap1=False)
            g.blob(head + np.array((0, 0, 0.1)), (0.045, 0.045, 0.12), "grain", sub=(1, 1, 0)[lod], sway=1.0, seed=i * 31 + j, rough=0.35)
            for q in range((7, 4, 2)[lod]):
                t = 0.15 + 0.7 * q / 7
                b = foot + (P[-1] - foot) * t
                a = rng.uniform(0, TAU)
                out = np.array((math.cos(a), math.sin(a), 0.0))
                d = unit(out + np.array((0, 0, 0.9)))
                g.blade(b, d, unit(np.cross(d, out)), rng.uniform(0.6, 0.9), 0.05 * (1 if lod == 0 else 1.6), (3, 2, 1)[lod],
                        "leaf_dry" if (q < 2 and rng.random() < 0.6) else ("leaf_light" if rng.random() < 0.3 else "leaf"),
                        droop=rng.uniform(1.0, 1.8), fold=0.3, sway0=t, sway1=1.0, shape="strap")
    g.info = {"title": "Gaoliang (tall millet), 3 m square", "high_m": 3.4, "foot_m": size / 2}
    return g


BUILDERS = {
    "JungleGiant": jungle_giant, "JungleTree": jungle_tree, "CoconutPalm": coconut_palm, "Bamboo": bamboo,
    # three palm shapes for shores and airfields (12, 18 and 24 m; the tall one leans out over the water)
    "CoconutPalmYoung": lambda lod=0, seed=31: coconut_palm(lod, seed, H=11.0, lean=0.08, name="CoconutPalmYoung", title="Coconut palm, young"),
    "CoconutPalmTall": lambda lod=0, seed=32: coconut_palm(lod, seed, H=23.5, lean=0.3, name="CoconutPalmTall", title="Coconut palm, tall and leaning"),
    "Banana": banana, "Undergrowth": undergrowth, "ElephantGrass": tall_grass,
    "Reeds": lambda lod=0, seed=13: tall_grass(lod, seed, H=3.0, name="Reeds", plumes=14, wide=0.022, n=60, title="Reeds",
                                               paints={"grass": "7A8048", "grass_light": "92945A", "grass_dry": "AC9E6A", "seed": "8C7A64", "straw": "A89A6E"}),
    "HillPine": hill_pine, "Willow": willow, "Poplar": poplar, "RicePatch": rice_patch, "Gaoliang": gaoliang,
}
