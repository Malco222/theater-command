"""Buildings of the Asian battlefields, to the art director's town kits (world-look-brief.md 3.4), each of our own
design and none a real building:

- Thatch and tin (Burma's villages, Malaya, Siam, the Indies): a house raised on posts, woven cane walls (182, 156,
  112), a steep thatched roof (156, 130, 86) or one of corrugated tin (156, 162, 164), a veranda and a ladder.
- The Indian town's gilt-capped stepped tower, as Burma builds it: a whitewashed bell-shaped shrine on terraces with
  a gilt spire (the battle's `spire` landmark there).
- The walled town (China, Formosa, Manchuria, Korea): a farmhouse of grey brick (134, 130, 122) under grey-blue tile
  (92, 100, 108) with a curved ridge; a farmyard of three such ranges and a wall with a roofed gate; a tiered tower
  (the battle's `spire` landmark); North China's flat-roofed mud house; grave mounds and a straw stack for the fields.

Every builder takes lod 0, 1 or 2: the near one has the tile ribs, thatch tiers and fringe, frames and steps; the far
one is the block, the roof and the dark of the doors.
"""
import math

import numpy as np

from meshkit import TAU, Geo, bezier, leaf_cloud, unit

DARK = "2A2622"          # doorways, windows: the dark inside


# ================================================================================================ roofs
def hip_shell(g, x0, x1, y0, y1, ze, zr, inset, paint, sway=0.0, under=None, drop=0.0):
    """A hipped roof: eaves round the rectangle at ze, a ridge at zr inset from the ends."""
    xm0, xm1 = x0 + inset, x1 - inset
    ym = (y0 + y1) / 2
    v = [(x0, y0, ze), (x1, y0, ze), (x1, y1, ze), (x0, y1, ze), (xm0, ym, zr), (xm1, ym, zr)]
    faces = [[0, 1, 5, 4], [2, 3, 4, 5], [1, 2, 5], [3, 0, 4]]
    g.add(v, faces, paint, sway)
    if under:
        u = [(x0, y0, ze - 0.02), (x1, y0, ze - 0.02), (x1, y1, ze - 0.02), (x0, y1, ze - 0.02)]
        g.add(u, [[3, 2, 1, 0]], under, sway)


def thatch_roof(g, rng, x0, x1, y0, y1, ze, zr, lod, tiers=3, paint="thatch", paint2="thatch_old"):
    """Thatch: a thick hipped mass laid in courses, each course's lower lip standing proud of the one below it (the
    light catches the steps), the slopes bellied a little, rolled hips and ridge, and a ragged fringe of straw at the
    eaves. `tiers` is kept for old calls; the courses are set by the roof's height."""
    inset = (x1 - x0) * 0.22
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    xr0, xr1 = x0 + inset, x1 - inset
    # rings from the eave (k=0) to the ridge (k=1): corners of a rectangle shrinking to the ridge line
    courses = (max(5, int((zr - ze) / 0.32)), 4, 1)[lod]

    def ring(k, lip=0.0):
        ax0, ax1 = x0 + (xr0 - x0) * k, x1 + (xr1 - x1) * k
        ay0, ay1 = y0 + (cy - y0) * k, y1 + (cy - y1) * k
        z = ze + (zr - ze) * (k - 0.10 * math.sin(math.pi * k)) + lip * 0.08
        o = lip * 0.05
        return [(ax0 - o, ay0 - o, z), (ax1 + o, ay0 - o, z), (ax1 + o, ay1 + o, z), (ax0 - o, ay1 + o, z)]
    pick = lambda: paint2 if rng.random() < 0.35 else paint
    if lod == 2:
        hip_shell(g, x0, x1, y0, y1, ze, zr, inset, paint, under="thatch_under")
    else:
        for i in range(courses):
            k0, k1 = i / courses, (i + 1) / courses
            lo = ring(k0, lip=1.0 if i else 1.6)
            hi = ring(k1)
            v = lo + hi
            # four slopes of the course: quads lo[j], lo[j+1], hi[j+1], hi[j]
            g.add(v, [[j, (j + 1) % 4, 4 + (j + 1) % 4, 4 + j] for j in range(4)], pick(), 0.0)
            if i:
                # the step under the lip: from the course below's top ring out to this lip
                st = ring(k0) + lo
                g.add(st, [[j, (j + 1) % 4, 4 + (j + 1) % 4, 4 + j] for j in range(4)], "thatch_under", 0.0)
        # the eave's thickness and the underside
        e = ring(0.0, lip=1.6)
        eb = [(x, y, z - 0.28) for x, y, z in e]
        g.add(e + eb, [[4 + j, 4 + (j + 1) % 4, (j + 1) % 4, j] for j in range(4)], paint2, 0.0)
        g.add(eb, [[3, 2, 1, 0]], "thatch_under", 0.0)
        # ridge and hips rolled
        sides = (6, 4)[lod]
        rz = ring(1.0)[0][2]
        g.tube(np.array([(xr0 - 0.1, cy, rz + 0.05), (xr1 + 0.1, cy, rz + 0.05)]), 0.17, sides, paint2, cap0=True)
        for (ex, ey), rx in (((x0, y0), xr0), ((x1, y0), xr1), ((x1, y1), xr1), ((x0, y1), xr0)):
            P = np.array([(ex + (rx - ex) * t, ey + (cy - ey) * t, ze + (zr - ze) * (t - 0.10 * math.sin(math.pi * t)) + 0.04)
                          for t in np.linspace(0.04, 1.0, 6)])
            g.tube(P, 0.1, sides, paint2, cap0=True, cap1=False)
    if lod == 0:
        # the fringe: straw ends hanging at the eaves, two triangles each
        per = 9.0
        edge = []
        e = ring(0.0, lip=1.6)
        for j in range(4):
            (ax, ay, az), (bx, by, bz) = e[j], e[(j + 1) % 4]
            L = math.hypot(bx - ax, by - ay)
            for s_ in np.linspace(0, 1, int(L * per), endpoint=False):
                edge.append(((ax + (bx - ax) * s_, ay + (by - ay) * s_, az - 0.24), (bx - ax) / L, (by - ay) / L))
        n = len(edge)
        P = np.array([e_[0] for e_ in edge]) + np.column_stack([rng.normal(size=n) * 0.02, rng.normal(size=n) * 0.02, rng.uniform(-0.03, 0.03, n)])
        T = np.array([(e_[1], e_[2], 0.0) for e_ in edge])
        out = np.column_stack([T[:, 1], -T[:, 0], np.zeros(n)])
        Ls = rng.uniform(0.1, 0.26, n)[:, None]
        W = 0.13
        tip = P + out * 0.04 + np.array((0, 0, -1.0)) * Ls
        V = np.stack([P - T * W / 2, P + T * W / 2, tip + T * rng.normal(size=(n, 1)) * 0.03], 1)
        g.add_many(V, [[0, 1, 2]], [paint2 if rng.random() < 0.4 else paint for _ in range(n)], np.full((n, 3), 0.05))
    g.two_sided |= {paint, paint2}


def tin_roof(g, x0, x1, y0, y1, ze, zr, lod, paint="tin", rust="tin_rust"):
    """A gabled roof of corrugated iron sheets: ridges every 7.6 cm near, plain sheets far; a rusted sheet or two."""
    ym = (y0 + y1) / 2
    for s, (ya, yb) in enumerate(((y0, ym), (y1, ym))):
        nx = (int((x1 - x0) / 0.076), 1, 1)[lod]
        xs = np.linspace(x0, x1, nx * 2 + 1)
        zs = np.where(np.arange(len(xs)) % 2 == 0, 0.0, 0.019) if lod == 0 else np.zeros(len(xs))
        sheet = int((x1 - x0) / 0.8)
        for k in range(max(1, sheet)):
            a, b = x0 + (x1 - x0) * k / sheet, x0 + (x1 - x0) * (k + 1) / sheet
            sel = (xs >= a - 1e-6) & (xs <= b + 1e-6)
            xx, zz = xs[sel], zs[sel]
            if len(xx) < 2:
                xx, zz = np.array([a, b]), np.zeros(2)
            Lp = np.column_stack([xx, np.full(len(xx), ya), ze + zz])
            Rp = np.column_stack([xx, np.full(len(xx), yb), zr + zz])
            h = (k * 7 + s * 3) % 9
            pnt = rust if h == 4 else ("tin_dull" if h in (1, 6, 7) else paint)
            if s == 0:
                g.quad_strip(Lp, Rp, pnt)
            else:
                g.quad_strip(Rp, Lp, pnt)
        g.two_sided |= {paint, rust, "tin_dull"}
    # gable boards
    for x in (x0, x1):
        g.add([(x, y0, ze), (x, y1, ze), (x, ym, zr)], [[0, 1, 2]], "wood_dark")
    g.two_sided.add("wood_dark")


def tile_roof(g, x0, x1, y0, y1, ze, zr, lod, paint="tile", rib="tile_rib", under="wood_dark", sag=0.16, upturn=0.22,
              ridge_paint="tile_ridge"):
    """A Chinese gabled roof: each slope sags a little between eave and ridge, the eaves lift toward the corners,
    rows of rounded tiles run down the slope, a raised ridge whose ends curl up."""
    ym = (y0 + y1) / 2
    nu, nv = (24, 10, 4)[lod], (8, 4, 2)[lod]

    def slope(side):
        ya = y0 if side == 0 else y1

        def fn(u, v):
            x = x0 + (x1 - x0) * u
            y = ya + (ym - ya) * v
            e = abs(2 * u - 1) ** 6
            z = ze + (zr - ze) * (v - sag * math.sin(math.pi * v)) + upturn * e * (1 - v) ** 2
            return (x, y, z)
        return fn
    for side in (0, 1):
        fn = slope(side)
        # each slope's underside runs u the other way to its tiles, so its faces point down
        if side == 0:
            g.grid(fn, nu, nv, paint)
            g.grid(lambda u, v, fn=fn: (fn(1 - u, v)[0], fn(1 - u, v)[1], fn(1 - u, v)[2] - 0.1), nu, nv, under)
        else:
            g.grid(lambda u, v, fn=fn: fn(1 - u, v), nu, nv, paint)
            g.grid(lambda u, v, fn=fn: (fn(u, v)[0], fn(u, v)[1], fn(u, v)[2] - 0.1), nu, nv, under)
        if lod == 0:
            nr = int((x1 - x0) / 0.24)
            for i in range(nr):
                u = (i + 0.5) / nr
                P = np.array([fn(u, v) for v in np.linspace(0, 0.97, 7)]) + np.array((0, 0, 0.035))
                g.tube(P, 0.045, 4, rib, cap0=True, cap1=False)
        # the eave's edge board
        P = np.array([fn(u, 0.0) for u in np.linspace(0, 1, nu + 1)])
        g.tube(P, 0.06, (5, 4, 3)[lod], rib, cap0=True)
    # the ridge with curled ends
    n = (16, 8, 4)[lod]
    P = np.array([(x0 + (x1 - x0) * u, ym, zr + 0.12 + 0.28 * abs(2 * u - 1) ** 8) for u in np.linspace(-0.01, 1.01, n)])
    g.tube(P, 0.14, (8, 5, 4)[lod], ridge_paint, cap0=True)
    # gable ends closed
    for x in (x0 + 0.25, x1 - 0.25):
        g.add([(x, y0 + 0.5, ze + 0.05), (x, y1 - 0.5, ze + 0.05), (x, ym, zr)], [[0, 1, 2]], "brick")
    g.two_sided.add("brick")


# ================================================================================================ walls
def opening(g, x, y, z0, z1, w, normal, paint=DARK, frame="wood_dark", lod=0, shutters=False):
    """A door or window: the dark opening laid just proud of the wall, with its frame (near)."""
    n = np.array((*normal, 0.0)[:3], float)
    t = np.array((-n[1], n[0], 0.0))
    c = np.array((x, y, 0.0)) + n * 0.012
    v = [c - t * w / 2 + (0, 0, z0), c + t * w / 2 + (0, 0, z0), c + t * w / 2 + (0, 0, z1), c - t * w / 2 + (0, 0, z1)]
    g.add(v, [[0, 1, 2, 3]], "opening")
    if lod == 0:
        for a, b in (((-w / 2, z0), (-w / 2, z1)), ((w / 2, z0), (w / 2, z1)), ((-w / 2, z1), (w / 2, z1)), ((-w / 2, z0), (w / 2, z0))):
            p0 = c + t * a[0] + (0, 0, a[1]) + n * 0.02
            p1 = c + t * b[0] + (0, 0, b[1]) + n * 0.02
            g.tube(np.array([p0, p1]), 0.04, 4, frame, cap0=True)


# ================================================================================================ thatch and tin
def stilt_house(lod=0, seed=21, L=7.0, D=5.0, roof="thatch", veranda=True):
    """A village house on posts (Burma, Malaya): the floor 1.8 m up on a grid of hardwood posts, woven cane walls on a
    timber frame, a steep roof, a veranda across the front and a ladder up to it."""
    rng = np.random.default_rng(seed)
    g = Geo("StiltHouse" if roof == "thatch" else "StiltHouseTin",
            {"wood": "6E5A44", "wood_dark": "4A3C30", "weave": "B69C70", "weave_dark": "9A8258", "thatch": "9C8256",
             "thatch_old": "86704C", "thatch_under": "5E5040", "tin": "9CA2A4", "tin_dull": "7A7E7C", "tin_rust": "8A5A3E", "opening": DARK,
             "bamboo": "A49860", "earth": "8C6448"})
    hl, hd = L / 2, D / 2
    zf, zw = 1.8, 1.8 + 2.3                   # floor, wall top
    # posts
    for x in np.linspace(-hl + 0.15, hl - 0.15, 4):
        for y in ([-hd + 0.15, hd - 0.15] + ([-hd - 1.6] if veranda else [])):
            lean = rng.normal(size=2) * 0.02
            g.tube(np.array([(x, y, -0.2), (x + lean[0], y + lean[1], zf + (2.3 if y > -hd - 1 else 0.9))]), 0.09, (7, 5, 4)[lod], "wood",
                   cap1=True, rough=(0.08, 2) if lod == 0 else None, seed=int(x * 10 + y))
    # floor platform and joists
    g.box((-hl - 0.1, -hd - 0.1, zf - 0.12), (hl + 0.1, hd + 0.1, zf), "wood_dark")
    if lod == 0:
        for x in np.linspace(-hl, hl, 9):
            g.box((x - 0.05, -hd - 0.2, zf - 0.3), (x + 0.05, hd + 0.2, zf - 0.12), "wood_dark")
    # walls: woven panels in a frame
    wall = [(-hl, -hd), (hl, -hd), (hl, hd), (-hl, hd)]
    g.prism(wall, zf, zw, "weave", top=False)
    if lod == 0:
        # the weave shows as faint horizontal battens and upright frame members
        for z in np.linspace(zf + 0.4, zw - 0.2, 4):
            g.prism([(-hl - 0.03, -hd - 0.03), (hl + 0.03, -hd - 0.03), (hl + 0.03, hd + 0.03), (-hl - 0.03, hd + 0.03)], z, z + 0.05, "bamboo", top=False)
        for x in np.linspace(-hl, hl, 5):
            for y in (-hd - 0.04, hd + 0.04):
                g.box((x - 0.05, y - 0.03, zf), (x + 0.05, y + 0.03, zw), "wood")
    # door and windows
    opening(g, -hl * 0.3, -hd, zf + 0.02, zf + 1.85, 0.9, (0, -1), lod=lod)
    for x in (hl * 0.4,):
        opening(g, x, -hd, zf + 0.9, zf + 1.7, 1.0, (0, -1), lod=lod)
    for y in (0.0,):
        opening(g, hl, y, zf + 0.9, zf + 1.6, 0.9, (1, 0), lod=lod)
        opening(g, -hl, y, zf + 0.9, zf + 1.6, 0.9, (-1, 0), lod=lod)
    if veranda:
        g.box((-hl - 0.1, -hd - 1.8, zf - 0.32), (hl + 0.1, -hd, zf - 0.2), "wood_dark")
        if lod == 0:
            for x in np.linspace(-hl, hl, 12):          # rail
                g.box((x - 0.03, -hd - 1.75, zf - 0.2), (x + 0.03, -hd - 1.7, zf + 0.7), "bamboo")
            g.box((-hl, -hd - 1.76, zf + 0.65), (hl, -hd - 1.68, zf + 0.72), "bamboo")
        # ladder
        x = hl * 0.55
        for s in (-0.25, 0.25):
            g.tube(np.array([(x + s, -hd - 3.0, 0.0), (x + s, -hd - 1.8, zf - 0.2)]), 0.04, (5, 4, 3)[lod], "bamboo", cap1=True)
        if lod < 2:
            for k in range(1, 6):
                t = k / 6
                g.tube(np.array([(x - 0.25, -hd - 3.0 + 1.2 * t, (zf - 0.2) * t), (x + 0.25, -hd - 3.0 + 1.2 * t, (zf - 0.2) * t)]), 0.025, 4, "bamboo")
    # roof
    ov = 0.9
    y0 = -hd - (2.0 if veranda else ov)
    if roof == "thatch":
        thatch_roof(g, rng, -hl - ov, hl + ov, y0, hd + ov, zw - 0.1, zw + 3.0, lod)
    else:
        tin_roof(g, -hl - ov, hl + ov, y0, hd + ov, zw - 0.15, zw + 2.2, lod)
        # gable walls under a tin roof
        ym = (y0 + hd + ov) / 2
        for x in (-hl, hl):
            g.add([(x, -hd, zw), (x, hd, zw), (x, ym, zw + 2.0)], [[0, 1, 2]], "weave")
        g.two_sided.add("weave")
    g.info = {"title": "House on posts" + ("" if roof == "thatch" else ", tin roof"), "high_m": round(zw + 3.2, 1), "foot_m": round(max(L, D + 3) / 2 + 1, 1)}
    return g


def shrine(lod=0, seed=22, H=16.0):
    """The gilt-capped stepped tower of the Indian town kit as Burma builds it (our own design, no real shrine): three
    square terraces, a whitewashed bell, banded rings narrowing to a lotus bud, and a gilt spire with its umbrella."""
    g = Geo("Shrine", {"plaster": "E2DCCC", "plaster_shade": "C8C0AC", "gilt": "C49A44", "gilt_dark": "8C6A2E", "stone": "8E8678"})
    s = H / 16
    z = 0.0
    for k, (w, h) in enumerate(((7.0, 0.9), (5.8, 0.8), (4.8, 0.7))):
        w *= s
        h *= s
        g.box((-w / 2, -w / 2, z), (w / 2, w / 2, z + h), "plaster" if k else "stone")
        if lod == 0:
            q, t = w / 2 + 0.06, 0.32 * s
            for lo, hi in (((-q, -q), (q, -q + t)), ((-q, q - t), (q, q)), ((-q, -q + t), (-q + t, q - t)), ((q - t, -q + t), (q, q - t))):
                g.box((lo[0], lo[1], z + h - 0.12 * s), (hi[0], hi[1], z + h + 0.05 * s), "plaster_shade")
            # small corner stupas
            for cx in (-1, 1):
                for cy in (-1, 1):
                    g.lathe([(0.3 * s, 0), (0.28 * s, 0.4 * s), (0.18 * s, 0.7 * s), (0.04 * s, 1.1 * s)], 10, "plaster",
                            at=(cx * (w / 2 - 0.35 * s), cy * (w / 2 - 0.35 * s), z + h), cap_top=False)
        z += h
    sides = (32, 16, 8)[lod]
    prof = [(2.1, 0.0), (2.2, 0.25), (2.0, 0.45), (2.05, 0.6), (1.9, 0.9), (1.75, 1.6), (1.45, 2.4), (1.0, 3.1), (0.7, 3.4),
            (0.72, 3.6), (0.62, 3.7), (0.64, 3.9), (0.54, 4.0), (0.56, 4.2), (0.46, 4.3), (0.5, 4.6), (0.42, 5.1), (0.3, 5.6),
            (0.16, 6.0)]
    g.lathe([(r * s, h * s) for r, h in prof], sides, "plaster", at=(0, 0, z), cap_top=False)
    zt = z + 6.0 * s
    g.lathe([(0.16 * s, 0), (0.14 * s, 0.7 * s), (0.05 * s, 1.4 * s), (0.01 * s, 1.7 * s)], (12, 8, 5)[lod], "gilt", at=(0, 0, zt))
    if lod < 2:
        g.lathe([(0.05 * s, 0.0), (0.5 * s, 0.1 * s), (0.42 * s, 0.22 * s), (0.04 * s, 0.35 * s)], (16, 10)[lod], "gilt_dark",
                at=(0, 0, zt + 0.55 * s))
        g.lathe([(0.04 * s, 0.0), (0.32 * s, 0.08 * s), (0.27 * s, 0.17 * s), (0.03 * s, 0.26 * s)], (16, 10)[lod], "gilt_dark",
                at=(0, 0, zt + 1.0 * s))
    # stairs up the middle of each face
    if lod == 0:
        for a in range(4):
            c, sn = math.cos(a * math.pi / 2), math.sin(a * math.pi / 2)
            for k in range(8):
                d = 3.5 * s + 0.35 * s * (7 - k) / 7 * 0 + 0.3 * s
                x0, x1 = 3.5 * s + 0.6 * s - k * 0.12 * s, 3.5 * s + 0.6 * s - (k + 1) * 0.12 * s
                lo = np.array((x1, -0.7 * s, 0.0))
                hi = np.array((x0, 0.7 * s, (k + 1) * 0.29 * s))
                M = np.array([[c, -sn, 0], [sn, c, 0], [0, 0, 1]])
                g.box(lo, hi, "stone", M=M, at=(0, 0, 0))
    g.info = {"title": "Gilt-capped shrine (Burma's landmark)", "high_m": round(zt + 1.8 * s, 1), "foot_m": round(4.2 * s, 1)}
    return g


# ================================================================================================ walled town
def china_house(lod=0, seed=31, L=10.0, D=5.2, walls="brick", roof="tile", name="ChinaHouse", title="Farmhouse"):
    """A one-storey range of a Chinese farmhouse: grey brick on a stone plinth, a double door in the middle under a
    lintel, lattice windows either side, a curved tile roof with a raised ridge."""
    pal = {"brick": "86827A", "brick_dark": "6E6A64", "stone": "8E8A80", "tile": "5C646C", "tile_rib": "4E565E",
           "tile_ridge": "4A5058", "wood_dark": "3E342C", "wood": "6A5444", "opening": DARK, "lattice": "5A4A3C",
           "mud": "A8956E", "mud_dark": "8C7A58", "thatch": "9C8A5C", "thatch_old": "86764E", "thatch_under": "5E5040"}
    g = Geo(name, pal)
    hl, hd = L / 2, D / 2
    wallp = "brick" if walls == "brick" else "mud"
    g.box((-hl - 0.15, -hd - 0.15, 0.0), (hl + 0.15, hd + 0.15, 0.45), "stone" if walls == "brick" else "mud_dark")
    zw = 3.0
    g.prism([(-hl, -hd), (hl, -hd), (hl, hd), (-hl, hd)], 0.45, zw, wallp, top=False)
    if lod == 0 and walls == "brick":
        # the brick shows as courses: thin darker bands every seventh course, and quoins at the corners
        for i, z in enumerate(np.arange(0.45, zw - 0.2, 0.24)):
            a, b = (0.36, 0.2) if i % 2 == 0 else (0.2, 0.36)
            for sx in (-1, 1):
                for sy in (-1, 1):
                    cx, cy = sx * hl, sy * hd
                    g.box((min(cx, cx - sx * a) - 0.02, min(cy, cy - sy * b) - 0.02, z), (max(cx, cx - sx * a) + 0.02, max(cy, cy - sy * b) + 0.02, z + 0.22), "brick_dark")
        g.prism([(-hl - 0.04, -hd - 0.04), (hl + 0.04, -hd - 0.04), (hl + 0.04, hd + 0.04), (-hl - 0.04, hd + 0.04)], zw - 0.32, zw, "brick_dark", top=False)
    # front: door and two windows; back: one small window
    opening(g, 0.0, -hd, 0.45, 2.55, 1.5, (0, -1), lod=lod)
    for x in (-hl * 0.55, hl * 0.55):
        opening(g, x, -hd, 1.3, 2.3, 1.3, (0, -1), paint="lattice", lod=lod)
        if lod == 0:
            for k in range(1, 6):
                xx = x - 0.65 + 1.3 * k / 6
                g.box((xx - 0.015, -hd - 0.04, 1.3), (xx + 0.015, -hd - 0.02, 2.3), "wood_dark")
            for zz in (1.63, 1.96):
                g.box((x - 0.65, -hd - 0.04, zz - 0.015), (x + 0.65, -hd - 0.02, zz + 0.015), "wood_dark")
    opening(g, 0.0, hd, 1.6, 2.2, 0.7, (0, 1), lod=lod)
    if lod == 0:
        g.box((-1.0, -hd - 0.12, 2.55), (1.0, -hd + 0.02, 2.75), "wood_dark")          # lintel
        g.box((-1.1, -hd - 1.0, 0.0), (1.1, -hd - 0.15, 0.3), "stone")                # step
    ov = 0.7
    if roof == "tile":
        tile_roof(g, -hl - ov, hl + ov, -hd - ov, hd + ov, zw - 0.05, zw + 2.4, lod)
    elif roof == "flat":
        # North China: a low mud roof barely sloping, a parapet lip
        ya, yb, xa, xb = -hd - 0.45, hd + 0.3, -hl - 0.12, hl + 0.12
        arch = lambda v: zw + 0.1 + 0.55 * math.sin(math.pi * v)
        g.grid(lambda u, v: (xa + (xb - xa) * u, ya + (yb - ya) * v, arch(v)), 2, (10, 6, 3)[lod], "mud_dark")
        g.prism([(xa, ya), (xb, ya), (xb, yb), (xa, yb)], zw - 0.15, zw + 0.1, "mud_dark", top=False)
        nv = (10, 6, 3)[lod]
        vs = np.linspace(0, 1, nv + 1)
        for x0_, x1_ in ((xa - 0.1, xa + 0.25), (xb - 0.25, xb + 0.1)):
            # the gable wall carried up a hand's breadth above the roof, its top following the arch
            top = [(ya + (yb - ya) * v, arch(v) + 0.22) for v in vs]
            n = len(top)
            V = [(x0_, y, z) for y, z in top] + [(x1_, y, z) for y, z in top] + [(x0_, yb, zw - 0.15), (x0_, ya, zw - 0.15), (x1_, yb, zw - 0.15), (x1_, ya, zw - 0.15)]
            F = [[i, n + i, n + i + 1, i + 1] for i in range(n - 1)]                       # the top
            F.append([2 * n + 1] + list(range(n)) + [2 * n])                              # outer face (x0 side)
            F.append([2 * n + 2] + list(range(2 * n - 1, n - 1, -1)) + [2 * n + 3])
            F.append([n - 1, 2 * n - 1, 2 * n + 2, 2 * n])                                # back end
            F.append([2 * n + 1, 2 * n + 3, n, 0])                                        # front end
            g.add(V, F, "mud")
        g.box((hl * 0.55 - 0.3, 0.3, zw), (hl * 0.55 + 0.3, 0.9, zw + 1.45), "mud")
        g.box((hl * 0.55 - 0.36, 0.24, zw + 1.45), (hl * 0.55 + 0.36, 0.96, zw + 1.55), "mud_dark")
    else:
        rng = np.random.default_rng(seed)
        thatch_roof(g, rng, -hl - ov, hl + ov, -hd - ov, hd + ov, zw - 0.05, zw + 2.2, lod)
    g.info = {"title": title, "high_m": round(zw + 3.0, 1), "foot_m": round(hl + 1.0, 1)}
    return g


def farmyard(lod=0, seed=32):
    """A farmyard of the walled-town kit: the main range facing south, two side ranges, a brick wall round the yard
    with a small roofed gate."""
    g = china_house(lod, seed, 11.0, 5.4, name="Farmyard", title="Farmyard (three ranges and a walled yard)")
    V0 = g.nv
    for sx in (-1, 1):
        side = china_house(lod, seed + sx, 7.0, 4.2)
        # turn the side range to face the yard and set it at the side
        M = np.array([[0, -sx, 0], [sx, 0, 0], [0, 0, 1]], float)
        Vs, Ss, Fs, Fps = side.arrays()
        Vs = Vs @ M.T + np.array((sx * 7.2, -6.3, 0.0))
        base = g.nv
        g.V.append(Vs)
        g.S.append(Ss)
        for f, p in zip(Fs, Fps):
            g.F.append([base + i for i in f])
            g.Fp.append(g.paint_names.index(side.paint_names[p]))
        g.nv += len(Vs)
        g.two_sided |= side.two_sided
    # yard wall: from the side ranges round the front, a gate in the middle
    z1 = 2.2
    for a, b in (((-9.5, -10.5), (-1.3, -10.5)), ((1.3, -10.5), (9.5, -10.5)), ((-9.5, -10.5), (-9.5, -9.8)), ((9.5, -10.5), (9.5, -9.8))):
        (ax, ay), (bx, by) = a, b
        dx, dy = bx - ax, by - ay
        Ln = math.hypot(dx, dy)
        nx, ny = -dy / Ln * 0.2, dx / Ln * 0.2
        g.prism([(ax - nx, ay - ny), (bx - nx, by - ny), (bx + nx, by + ny), (ax + nx, ay + ny)], 0.0, z1, "brick")
        if lod < 2:
            g.prism([(ax - nx * 1.8, ay - ny * 1.8), (bx - nx * 1.8, by - ny * 1.8), (bx + nx * 1.8, by + ny * 1.8), (ax + nx * 1.8, ay + ny * 1.8)], z1, z1 + 0.18, "tile_ridge")
    # the gate: two piers and a little tiled roof
    for x in (-1.3, 1.3):
        g.box((x - 0.3, -10.8, 0.0), (x + 0.3, -10.2, 2.8), "brick")
    tile_roof(g, -1.9, 1.9, -11.3, -9.7, 2.75, 3.6, lod, sag=0.1, upturn=0.25)
    g.add([(-1.0, -10.5, 0.0), (1.0, -10.5, 0.0), (1.0, -10.5, 2.5), (-1.0, -10.5, 2.5)], [[0, 1, 2, 3]], "opening")
    g.info = {"title": "Farmyard (three ranges and a walled yard)", "high_m": 5.6, "foot_m": 11.0}
    return g


def tiered_tower(lod=0, seed=33, storeys=7, H=30.0):
    """The walled-town kit's tiered tower (the battle's `spire` landmark in China; our own design, no real tower): an
    eight-sided brick shaft narrowing as it rises, a tiled eave flaring out at every storey, an arched opening on
    alternate faces, and an iron finial."""
    g = Geo("TieredTower", {"brick": "8E887C", "brick_dark": "6E6A62", "stone": "8E8A80", "tile": "5C646C", "tile_rib": "4E565E",
                            "opening": DARK, "metal": "4A4844"})
    sides = 8
    z = 0.0
    r0 = H * 0.135
    g.lathe([(r0 * 1.35, 0.0), (r0 * 1.35, 1.2)], sides, "stone", rot_z=math.pi / 8)
    z = 1.2
    sh = (H - 4.0) / storeys
    for k in range(storeys):
        f = k / storeys
        r = r0 * (1 - 0.36 * f)
        r2 = r0 * (1 - 0.36 * (k + 1) / storeys)
        h = sh * (1.15 - 0.3 * f)
        g.lathe([(r, 0.0), (r2 * 1.02, h * 0.82)], sides, "brick", at=(0, 0, z), cap_top=False, rot_z=math.pi / 8)
        # the eave: a flared tiled skirt
        ze = z + h * 0.82
        out = r2 * 1.02 + 1.2 + 0.5 * (1 - f)
        # the bracket band under the eave
        g.lathe([(r2 * 1.03, -0.75), (r2 * 1.1, -0.45), (r2 * 1.18, -0.2), (r2 * 1.22, -0.02)], sides, "brick_dark", at=(0, 0, ze), cap_top=False, rot_z=math.pi / 8)
        nseg = (3, 2, 1)[lod]
        prof = [(r2 * 1.02, 0.0)] + [(r2 * 1.02 + (out - r2 * 1.02) * t, -0.25 * t * t + 0.05) for t in np.linspace(0.3, 1.0, nseg)]
        g.lathe(prof, sides, "tile_rib", at=(0, 0, ze), cap_top=False, rot_z=math.pi / 8)     # (outward: faces down)
        g.lathe([(out, -0.25), (out - 0.05, -0.12), (r2 * 0.98, 0.25), (r2 * 0.95, h * 0.18)], sides, "tile", at=(0, 0, ze), cap_top=False, rot_z=math.pi / 8)
        if lod < 2:
            for a in range(0, sides, 2):
                ang = (a + 0.5) * TAU / sides + math.pi / 8 - TAU / 16
                ang = a * TAU / sides + math.pi / 8 + TAU / 16
                n = (math.cos(ang), math.sin(ang))
                rr = (r + r2) / 2 * math.cos(math.pi / sides)
                opening(g, n[0] * rr, n[1] * rr, z + h * 0.25, z + h * 0.6, r * 0.45, n, lod=2)
        z += h
    # the top storey's roof, closing the shaft (the finial stands on it)
    g.lathe([(r2 * 0.95, 0.0), (r2 * 0.6, 0.3), (0.35, 0.45)], sides, "tile", at=(0, 0, z), cap_top=True, rot_z=math.pi / 8)
    g.lathe([(0.25, 0.0), (0.18, 1.0), (0.32, 1.2), (0.06, 1.6), (0.04, 2.6), (0.0, 2.9)], (10, 6, 4)[lod], "metal", at=(0, 0, z + 0.2))
    g.info = {"title": "Tiered tower (China's landmark)", "high_m": round(z + 3.1, 1), "foot_m": round(r0 * 1.4, 1)}
    return g


def grave_mound(lod=0, seed=34):
    """A grave mound of the Chinese fields: a low earth cone with a small upright stone before it (cover from fire
    the men will use)."""
    g = Geo("GraveMound", {"earth": "7E6A4E", "earth_grass": "72704A", "stone": "8E8A80"})
    rng = np.random.default_rng(seed)
    g.blob((0, 0, 0.0), (1.4, 1.4, 1.1), "earth_grass", sub=(2, 1, 1)[lod], sway=0.0, seed=seed, rough=0.08, flat_bottom=0.0,
           mottle=[("earth_grass", 0.8), ("earth", 0.2)] if lod < 2 else None)
    if lod < 2:
        g.box((-0.28, -1.62, 0.0), (0.28, -1.5, 0.78), "stone")
        g.box((-0.4, -1.75, 0.0), (0.4, -1.4, 0.12), "stone")
    g.info = {"title": "Grave mound", "high_m": 1.1, "foot_m": 1.5}
    return g


def straw_stack(lod=0, seed=35):
    """A stack of rice straw round a pole, as it stood in every threshing yard."""
    g = Geo("StrawStack", {"straw": "B09A64", "straw_old": "8E7C52", "wood": "5E4E3E"})
    sd = (20, 10, 6)[lod]
    g.lathe([(0.9, 0.0), (1.05, 0.25), (1.12, 1.0), (1.1, 1.9), (1.0, 2.2)], sd, "straw", cap_top=False)
    g.lathe([(1.0, 2.0), (1.45, 2.05), (1.3, 2.4), (0.95, 2.85), (0.5, 3.25), (0.18, 3.45), (0.08, 3.6)], sd, "straw_old", cap_top=True)
    if lod < 2:
        g.lathe([(0.2, 3.45), (0.28, 3.55), (0.12, 3.75)], 8, "straw", cap_top=True)
    g.tube(np.array([(0, 0, 3.5), (0, 0, 4.1)]), 0.05, 5, "wood")
    g.info = {"title": "Straw stack", "high_m": 4.1, "foot_m": 1.5}
    return g


BUILDERS = {
    "StiltHouse": stilt_house,
    "StiltHouseTin": lambda lod=0, seed=23: stilt_house(lod, seed, L=8.0, D=5.5, roof="tin", veranda=False),
    "Shrine": shrine,
    "ChinaHouse": china_house,
    "NorthHouse": lambda lod=0, seed=36: china_house(lod, seed, 9.0, 5.0, walls="mud", roof="flat", name="NorthHouse", title="North China mud house"),
    "ThatchHouse": lambda lod=0, seed=37: china_house(lod, seed, 8.0, 4.8, walls="mud", roof="thatch", name="ThatchHouse", title="Mud house under thatch"),
    "Farmyard": farmyard,
    "TieredTower": tiered_tower,
    "GraveMound": grave_mound,
    "StrawStack": straw_stack,
}
