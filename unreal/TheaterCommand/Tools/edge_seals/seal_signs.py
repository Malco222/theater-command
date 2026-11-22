#!/usr/bin/env python3
"""The seals' order signs (release 2, "news at the map's edge", patch 0007).

The art director's ruling of 22 November, 06:45Z: one seal set, drawn for 1920 by 1080, used on every screen. The
thinnest stroke about 16 of the picture's 192 pixels (1.5 px there), and the gaps and counters as wide; thickened by
OFFSETTING the stroke, never by scaling the shape; round caps kept; where an inner detail would close up it is dropped
or merged, never shrunk.

Made from the order signs' own vector source, the art director's paths in src/campaign/ui/ordersigns.ts (the city
order badges' signs; Content/CityOrders/order-<key>.png are those paths drawn eight times their 24 by 16 box). For each
pressable sign (all but "star", "night" and "day": see REFUSED):

1. Its path, flattened, with the few edits listed in EDITS (a detail dropped, or two parts merged, where the offset
   would close the gap between them), and laid in its 192 by 128 picture as the badge lays it: its outline drawn a
   little smaller about its middle (the share, 0.863 to 0.909, or 1 for mtn) where the offset would otherwise run off
   the picture's edge, and centred where the badge's whole sign is centred, so that a sign that lost a part (eng's river,
   inf's rifle, mtn's flag) does not sit off-centre in the seal.
2. Offset by R = 8 px (of 192): the Minkowski sum with a disc of radius 8, every point within 8 px of the shape. So
   every stroke is its old width plus 16, and every point and corner a round cap 16 across.
3. Closed by C = 8 px: any gap or counter narrower than 16 px that is left is filled (merged, or the counter dropped),
   and an inner corner takes a fillet of radius 8, so that no gap is under 16 px.
4. Drawn white on clear (the badges' own colours, so the layer's tint still works), the offset's edge antialiased
   from its exact distance.

Then every picture is measured as the test measures it (measure(), the same steps as the C++ in edge_seals_test.cpp):
its thinnest stroke and its narrowest gap or counter, in px of 192. The table goes into EdgeSeals.cpp between the
generated markers (--write-table), and the test measures the pictures again and holds the table to them. Each
picture's alpha, row by row, is summed as CRC-32 (alpha_sum()), and the sums go into edge_seals_test.cpp between their
own markers: the test and --check hold every picture to its sum, so a picture can change only with the drawing that
made it (the measures alone would pass a picture of another drawing that happens to measure the same).

Usage (from anywhere; PIL and numpy only, deterministic):
  python3 Tools/edge_seals/seal_signs.py               # draw Content/CityOrders/Seal/seal-<key>.png and print measures
  python3 Tools/edge_seals/seal_signs.py --write-table  # and write the measures and the sums (EdgeSeals.cpp, the test)
  python3 Tools/edge_seals/seal_signs.py --check        # draw nothing: measure and sum the pictures, compare both
"""

from __future__ import annotations

import argparse
import math
import re
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]  # unreal/TheaterCommand
REPO = GAME.parents[1]
SIGNS_TS = REPO / "src" / "campaign" / "ui" / "ordersigns.ts"
ORDERS = GAME / "Content" / "CityOrders"
SEALS = ORDERS / "Seal"
EDGE_SEALS_CPP = GAME / "Source" / "TheaterCommand" / "Campaign" / "EdgeSeals.cpp"
EDGE_SEALS_TEST = HERE / "edge_seals_test.cpp"

W, H = 192, 128  # the picture, as the badges are drawn
UNIT = 8.0  # the signs' 24 by 16 box to the picture's 192 by 128
FLOOR = 16.0  # px of 192 (EdgeSeals' SealFloor): 1.5 units in a sign 18 wide, so 1.67 px at 1920 by 1080 (1.11 px a unit)
              # and 1.5 px from a window 972 high (a pixel a unit), where the whole set is pressed
R = FLOOR / 2.0  # the offset
C = FLOOR / 2.0  # the closing
EDGE_ROOM = 0.75  # px left clear at the picture's edge, past the antialiased edge

# The keys the edge's seals may press: every order sign but the star and the night raid's moon and star (the ruling's
# section 2), and the day raid's sun (the art director's call of 22 November, 08:18Z: merged at this weight it reads as
# a cog, which on a map of factories says industry; no new sun is drawn). A refused sign has no seal picture, and its
# seal takes the drawn mark (EdgeSeals::MayPress). (The raids' own signs are the raid card's: no row or line of the
# month card is signed "day" or "night" today, since a raid's row is signed bmr by day or by night, so the refusal
# keeps the sun and the moon out of the wax if one ever is.)
PRESSABLE = ["aa", "arm", "art", "bmr", "cav", "cv", "eng", "ftr", "hull", "inf", "mot", "mtn", "para", "ss", "tp"]
REFUSED = ["star", "night", "day"]


# ---- The vector source --------------------------------------------------------------------------------------------

def read_sources() -> dict[str, str]:
    """The art director's paths, by key, from the web build's own file (one source for both builds)."""
    text = SIGNS_TS.read_text(encoding="utf-8")
    paths: dict[str, str] = {}
    for key, d in re.findall(r"^\s+(\w+): '([^']+)',", text, re.M):
        paths[key] = d
    for key, d in re.findall(r"^ORDER_SIGN\.(\w+) = '([^']+)';", text, re.M):
        paths[key] = d
    missing = [k for k in PRESSABLE if k not in paths]
    if missing:
        sys.exit(f"seal_signs: no vector source for {', '.join(missing)} (looked in {SIGNS_TS})")
    return paths


_TOKEN = re.compile(r"[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def _arc(p0, rx, ry, phi_deg, large, sweep, p1):
    """An SVG arc as points after p0 (the endpoint-to-centre conversion of SVG 1.1, F.6.5)."""
    x1, y1 = p0
    x2, y2 = p1
    if rx == 0 or ry == 0:
        return [p1]
    phi = math.radians(phi_deg)
    cp, sp = math.cos(phi), math.sin(phi)
    dx, dy = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    x1p, y1p = cp * dx + sp * dy, -sp * dx + cp * dy
    rx, ry = abs(rx), abs(ry)
    lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if lam > 1.0:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    co = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        co = -co
    cxp, cyp = co * rx * y1p / ry, -co * ry * x1p / rx
    cx, cy = cp * cxp - sp * cyp + (x1 + x2) / 2.0, sp * cxp + cp * cyp + (y1 + y2) / 2.0

    def ang(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = ang(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(16, int(math.ceil(abs(dt) / (2 * math.pi) * 128)))
    out = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        out.append((cx + rx * math.cos(t) * cp - ry * math.sin(t) * sp, cy + rx * math.cos(t) * sp + ry * math.sin(t) * cp))
    out[-1] = (x2, y2)
    return out


def parse_path(d: str) -> list[np.ndarray]:
    """An SVG path's subpaths as closed polygons (curves flattened finely), in the path's own units."""
    toks = _TOKEN.findall(d)
    i, cmd = 0, None
    cur = (0.0, 0.0)
    start = (0.0, 0.0)
    last_ctrl = None  # (kind, point) for S and T
    polys: list[list[tuple[float, float]]] = []
    poly: list[tuple[float, float]] = []

    def num():
        nonlocal i
        v = float(toks[i])
        i += 1
        return v

    def flush():
        nonlocal poly
        if len(poly) >= 3:
            polys.append(poly)
        poly = []

    while i < len(toks):
        if re.fullmatch(r"[A-Za-z]", toks[i]):
            cmd = toks[i]
            i += 1
        elif cmd is None:
            raise ValueError("path starts with a number")
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = cur if rel else (0.0, 0.0)
        if c == "Z":
            flush()
            cur = start
            last_ctrl = None
            continue
        if c == "M":
            flush()
            cur = (ox + num(), oy + num())
            start = cur
            poly = [cur]
            cmd = "l" if rel else "L"  # (pairs after a moveto are linetos)
            last_ctrl = None
            continue
        if c == "L":
            cur = (ox + num(), oy + num())
            poly.append(cur)
            last_ctrl = None
        elif c == "H":
            cur = ((ox if rel else 0.0) + num(), cur[1])
            poly.append(cur)
            last_ctrl = None
        elif c == "V":
            cur = (cur[0], (oy if rel else 0.0) + num())
            poly.append(cur)
            last_ctrl = None
        elif c in "CS":
            if c == "C":
                c1 = (ox + num(), oy + num())
            else:
                c1 = (2 * cur[0] - last_ctrl[1][0], 2 * cur[1] - last_ctrl[1][1]) if last_ctrl and last_ctrl[0] == "C" else cur
            c2 = (ox + num(), oy + num())
            p = (ox + num(), oy + num())
            n = 48
            for k in range(1, n + 1):
                t = k / n
                a, b, e, f = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
                poly.append((a * cur[0] + b * c1[0] + e * c2[0] + f * p[0], a * cur[1] + b * c1[1] + e * c2[1] + f * p[1]))
            last_ctrl = ("C", c2)
            cur = p
        elif c in "QT":
            if c == "Q":
                q = (ox + num(), oy + num())
            else:
                q = (2 * cur[0] - last_ctrl[1][0], 2 * cur[1] - last_ctrl[1][1]) if last_ctrl and last_ctrl[0] == "Q" else cur
            p = (ox + num(), oy + num())
            n = 32
            for k in range(1, n + 1):
                t = k / n
                a, b, f = (1 - t) ** 2, 2 * (1 - t) * t, t * t
                poly.append((a * cur[0] + b * q[0] + f * p[0], a * cur[1] + b * q[1] + f * p[1]))
            last_ctrl = ("Q", q)
            cur = p
        elif c == "A":
            rx, ry, rot, large, sweep = num(), num(), num(), num(), num()
            p = (ox + num(), oy + num())
            poly.extend(_arc(cur, rx, ry, rot, int(large), int(sweep), p))
            cur = p
            last_ctrl = None
        else:
            raise ValueError(f"path command {cmd} not read")
    flush()
    return [np.array(p, dtype=np.float64) for p in polys]


# ---- Distances on a grid -------------------------------------------------------------------------------------------

def segments_of(polys: list[np.ndarray]) -> tuple[np.ndarray, np.ndarray]:
    a = np.concatenate([p for p in polys])
    b = np.concatenate([np.roll(p, -1, axis=0) for p in polys])
    keep = np.any(a != b, axis=1)
    return a[keep], b[keep]


def winding(a: np.ndarray, b: np.ndarray, xs: np.ndarray, ys: np.ndarray) -> np.ndarray:
    """The nonzero rule's inside, on the grid xs by ys (each row a scanline, the ray to +x)."""
    inside = np.zeros((len(ys), len(xs)), dtype=bool)
    y0, y1, x0, x1 = a[:, 1], b[:, 1], a[:, 0], b[:, 0]
    for j, y in enumerate(ys):
        up = (y0 <= y) & (y < y1)
        down = (y1 <= y) & (y < y0)
        hit = up | down
        if not hit.any():
            continue
        t = (y - y0[hit]) / (y1[hit] - y0[hit])
        xc = x0[hit] + t * (x1[hit] - x0[hit])
        dirs = np.where(up[hit], 1, -1)
        order = np.argsort(xc, kind="stable")
        xc, dirs = xc[order], dirs[order]
        right = np.concatenate([np.cumsum(dirs[::-1])[::-1], [0]])
        inside[j] = right[np.searchsorted(xc, xs, side="right")] != 0
    return inside


def _seg_dist(px: np.ndarray, py: np.ndarray, a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Each point's distance to its nearest segment (points by segments, broadcast)."""
    ax, ay = a[None, :, 0], a[None, :, 1]
    dx, dy = b[None, :, 0] - ax, b[None, :, 1] - ay
    qx, qy = px[:, None] - ax, py[:, None] - ay
    ll = dx * dx + dy * dy
    t = np.clip((qx * dx + qy * dy) / np.where(ll > 0, ll, 1.0), 0.0, 1.0)
    ex, ey = qx - t * dx, qy - t * dy
    return np.sqrt((ex * ex + ey * ey).min(axis=1))


def distance(a: np.ndarray, b: np.ndarray, xs: np.ndarray, ys: np.ndarray, limit: float) -> np.ndarray:
    """Each grid point's distance to the outline, exact where under limit (a lower bound over it). Tiles cull segments."""
    out = np.empty((len(ys), len(xs)))
    tile = 32
    for j0 in range(0, len(ys), tile):
        for i0 in range(0, len(xs), tile):
            tx, ty = xs[i0:i0 + tile], ys[j0:j0 + tile]
            cx, cy = (tx[0] + tx[-1]) / 2.0, (ty[0] + ty[-1]) / 2.0
            half = math.hypot(tx[-1] - tx[0], ty[-1] - ty[0]) / 2.0
            # (each segment's distance to the tile's middle: a segment farther than the nearest by more than the tile's
            # diagonal is no point's nearest)
            ax, ay = a[:, 0], a[:, 1]
            dx, dy = b[:, 0] - ax, b[:, 1] - ay
            ll = dx * dx + dy * dy
            t = np.clip(((cx - ax) * dx + (cy - ay) * dy) / np.where(ll > 0, ll, 1.0), 0.0, 1.0)
            dc = np.hypot(cx - ax - t * dx, cy - ay - t * dy)
            near = dc.min()
            if near - half > limit:
                out[j0:j0 + tile, i0:i0 + tile] = near - half
                continue
            pick = dc <= near + 2.0 * half + 1e-9
            gx, gy = np.meshgrid(tx, ty)
            out[j0:j0 + tile, i0:i0 + tile] = _seg_dist(gx.ravel(), gy.ravel(), a[pick], b[pick]).reshape(gy.shape)
    return out


BIG = 1 << 40


def sqdist_to(mask: np.ndarray, rmax: int) -> np.ndarray:
    """Each sample's squared distance (in samples) to the nearest True sample: exact up to rmax squared, else larger."""
    gh, gw = mask.shape
    xs = np.arange(gw, dtype=np.int64)
    left = np.where(mask, xs[None, :], -BIG)
    left = np.maximum.accumulate(left, axis=1)
    right = np.where(mask, xs[None, :], BIG)
    right = np.minimum.accumulate(right[:, ::-1], axis=1)[:, ::-1]
    g = np.minimum(xs[None, :] - left, right - xs[None, :])
    g = np.minimum(g, rmax + 1)
    g2 = g * g
    d2 = g2.copy()
    for dy in range(1, min(rmax, gh - 1) + 1):
        add = dy * dy
        np.minimum(d2[dy:], g2[:-dy] + add, out=d2[dy:])
        np.minimum(d2[:-dy], g2[dy:] + add, out=d2[:-dy])
    return d2


# ---- The measure (the same steps as edge_seals_test.cpp's MeasureSign) ------------------------------------------------

SS = 4  # samples a pixel, each way
PAD = 32  # px of clear ground round the picture, for the gaps' measure
NMAX = 512  # the widest measure, in eighths of a px: 64 px
DEPTH2 = 16  # a removed sliver deeper than 1 px (4 samples) is a stroke or a gap, not the antialiasing


def upsample(alpha: np.ndarray) -> np.ndarray:
    """The picture's ink at four samples a pixel each way (bilinear between pixel middles, ink where it is half or more),
    on a ground PAD px wide each side. All in integers, so that the C++ finds exactly the same samples."""
    h, w = alpha.shape
    a = alpha.astype(np.int64)

    def axis(n):
        k = np.arange(n * SS)
        u8 = 2 * k - 3  # the sample's place in eighths of a pixel, from the first pixel's middle
        i0 = np.floor_divide(u8, 8)
        w1 = u8 - 8 * i0
        return np.clip(i0, 0, n - 1), np.clip(i0 + 1, 0, n - 1), 8 - w1, w1

    x0, x1, wx0, wx1 = axis(w)
    y0, y1, wy0, wy1 = axis(h)
    s = (a[y0][:, x0] * wx0[None, :] + a[y0][:, x1] * wx1[None, :]) * wy0[:, None] + (a[y1][:, x0] * wx0[None, :] + a[y1][:, x1] * wx1[None, :]) * wy1[:, None]
    inner = 2 * s >= 16320
    out = np.zeros(((h + 2 * PAD) * SS, (w + 2 * PAD) * SS), dtype=bool)
    out[PAD * SS:(PAD + h) * SS, PAD * SS:(PAD + w) * SS] = inner
    return out


def measure(alpha: np.ndarray, gap: bool) -> float:
    """The thinnest stroke (gap False) or the narrowest gap or counter (gap True), in px of the picture: the widest T
    (in eighths of a px, to 64) for which opening the ink (or the ground) with a disc T across removes nothing deeper
    than 1 px. A stroke under T goes whole (as deep as half its width); a round cap or a fillet T across, or the
    antialiased edge, loses at most a sliver. So no stroke or gap is under the measure; but at a corner a disc wider than
    the cap passes while what it takes is under 1 px deep, so a cap 16 across reads 16 to about 23 by the corner's angle
    (a square corner about 21): the measure is a floor check, not the finest part's width (the screen uses FLOOR)."""
    ink = upsample(alpha)
    x = ~ink if gap else ink
    depth2 = sqdist_to(~x, NMAX // 4 + 2)

    def holds(n: int) -> bool:
        if n == 0:
            return True
        e = x & (16 * depth2 >= n * n)
        de2 = sqdist_to(e, n // 4 + 1)
        removed = x & ~(16 * de2 <= n * n)
        return not bool((depth2[removed] > DEPTH2).any())

    if holds(NMAX):
        return NMAX / 8.0
    lo, hi = 0, NMAX
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if holds(mid):
            lo = mid
        else:
            hi = mid
    return lo / 8.0


# ---- Drawing a seal sign -------------------------------------------------------------------------------------------

GPAD = 12  # px of ground round the picture while drawing, so the closing sees the clear ground beyond its edge


def place(polys: list[np.ndarray], whole: list[np.ndarray] | None = None) -> tuple[list[np.ndarray], float]:
    """The sign at the picture's scale (8 px a unit): its outline drawn a little smaller about its middle where the
    offset would run off the picture (the share it is drawn at, 1 where it has room; the offset then adds its 8 px all
    round, so the bolder sign keeps the badge's frame), and its box centred where the box of the badge's whole sign
    (whole, before any part was dropped) is centred."""
    px = [p * UNIT for p in polys]
    allp = np.concatenate(px)
    lo, hi = allp.min(axis=0), allp.max(axis=0)
    room = R + EDGE_ROOM
    k = min(1.0, (W - 2 * room) / (hi[0] - lo[0]), (H - 2 * room) / (hi[1] - lo[1]))
    mid = (lo + hi) / 2.0
    # (where the badge centres the whole sign: a sign that lost a part keeps that middle, not the one the part left)
    if whole is not None:
        everything = np.concatenate([p * UNIT for p in whole])
        target = (everything.min(axis=0) + everything.max(axis=0)) / 2.0
    else:
        target = mid
    # (scaled about its own middle and set on the target, then moved no more than it must to stand clear of the edge)
    new_lo, new_hi = target + (lo - mid) * k, target + (hi - mid) * k
    shift = np.zeros(2)
    for axis_, size in ((0, W), (1, H)):
        if new_lo[axis_] < room:
            shift[axis_] = room - new_lo[axis_]
        elif new_hi[axis_] > size - room:
            shift[axis_] = size - room - new_hi[axis_]
    return [target + (p - mid) * k + shift for p in px], k


def draw(polys_px: list[np.ndarray]) -> tuple[np.ndarray, dict]:
    """The offset and closed sign, white on clear, as 8-bit alpha (H by W); and what the closing filled."""
    a, b = segments_of(polys_px)
    # (the drawing grid: four samples a pixel each way, over the picture and GPAD px round it)
    gx = (np.arange((W + 2 * GPAD) * SS) + 0.5) / SS - GPAD
    gy = (np.arange((H + 2 * GPAD) * SS) + 0.5) / SS - GPAD
    inside = winding(a, b, gx, gy)
    d = np.where(inside, 0.0, distance(a, b, gx, gy, R + C + 1.0))
    # (closing: the ground a disc of radius C covers, where it fits clear of the offset shape (its middle R + C or more
    # from the shape), stays ground; the rest is ink. So the offset shape, with every gap it leaves under 2C filled and
    # every inner corner given a fillet of radius C)
    far = d >= R + C
    near2 = sqdist_to(far, int(math.ceil((C + 1.0) * SS)))
    reach = np.sqrt(near2) / SS
    filled = reach > C + 0.18  # (0.18: the grid's own error, at most half a sample's diagonal, kept off the offset's edge)
    offset = d <= R
    crop = (slice(GPAD * SS, (GPAD + H) * SS), slice(GPAD * SS, (GPAD + W) * SS))
    fill_alpha = filled[crop].reshape(H, SS, W, SS).mean(axis=(1, 3))
    # (the offset's edge, antialiased from its exact distance at each pixel's middle)
    cx = np.arange(W) + 0.5
    cy = np.arange(H) + 0.5
    cin = winding(a, b, cx, cy)
    cd = np.where(cin, 0.0, distance(a, b, cx, cy, R + 2.0))
    ramp = np.clip(0.5 + (R - cd), 0.0, 1.0)
    alpha = np.maximum(ramp, fill_alpha)
    out = np.round(alpha * 255.0).astype(np.uint8)
    info = {
        "filled": (filled & ~offset)[crop],  # what the closing added, at 4 samples a px
        "offset": offset[crop],
        "source": inside[crop],
    }
    return out, info


def save_png(alpha: np.ndarray, path: Path) -> None:
    rgba = np.zeros((alpha.shape[0], alpha.shape[1], 4), dtype=np.uint8)
    rgba[..., 0:3] = 255
    rgba[..., 3] = alpha  # (white throughout, clear pixels too, as the badges' own pictures are: the layer tints it)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, "RGBA").save(path, optimize=False)


# ---- What each seal sign drops or merges ---------------------------------------------------------------------------
# In the signs' own units (the 24 by 16 box, 8 px a unit): an offset of R is 1 unit, so a gap in the source under about
# 4.6 units (4 units over the share a sign is drawn at, place()) would close. Each such detail is dropped (a part of
# the path left out, by its place in the path) or merged (a piece added that joins two parts), never shrunk.

def ring(cx: float, cy: float, r0: float, r1: float) -> str:
    """An annulus as a path (outer ring one way, inner the other, so the nonzero rule leaves its middle clear)."""
    return (f"M{cx + r1} {cy}A{r1} {r1} 0 1 1 {cx - r1} {cy}A{r1} {r1} 0 1 1 {cx + r1} {cy}Z"
            f"M{cx + r0} {cy}A{r0} {r0} 0 1 0 {cx - r0} {cy}A{r0} {r0} 0 1 0 {cx + r0} {cy}Z")


# The art director's calls of 22 November, 08:18Z, took the three alternatives 0008 offered: aa's mount dropped (a gun
# on a stand, not a ramp), arm's gun lifted over a cut-back deck (the barrel clear over the notch says tank, and parts
# it from mot), tp's two funnels set apart (parts it from hull). day is refused (REFUSED) and has no entry.
EDITS: dict[str, dict] = {
    "aa": {"drop": [1, 2, 3, 8], "add": [],
           "short": "cradle, breech, recoil and square dropped",
           "note": "dropped: the cradle, the two slanted pieces under the barrel (breech and recoil) and the small square (merged, they made a solid wedge, a ramp at 20 px): the barrel stands on its mount block and base, a gun on a stand"},
    "arm": {"drop": [1, 3], "add": ["M2.05 5.71H16.0L18.4 8.36H0Z", "M5.43 4.9H15.32V5.8H5.43Z", "M2.89 8.3H19.06V9.2H2.89Z", "M14.47 1.37H24V3.18H14.47Z"],
            "short": "gun lifted, deck cut back; turret, hull and tracks merged",
            "note": "redrawn: the gun lifted to the turret's top line and the hull's deck cut back under it (19.66 to 16 units), so the barrel stands clear over a notch 16 or more wide; merged: the turret into the hull and the hull into the tracks (the gaps between them, 0.7 units, would close)"},
    "art": {"drop": [1], "add": [],
            "short": "hub dropped",
            "note": "dropped: the wheel's hub (its counter, 2.8 units across, would close): the wheel is solid"},
    "bmr": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only (the engines stand as bumps on the wings' leading edge)"},
    "cav": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only (the ear's point a round cap)"},
    "cv": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only"},
    "eng": {"drop": [1], "add": [],
            "short": "river dropped",
            "note": "dropped: the river (the gap under the piers, 1.5 units, would close; merged, bridge and river made one knot): the bridge, its two piers and its arch"},
    "ftr": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only (the nose and the tips round caps)"},
    "hull": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only"},
    "inf": {"drop": [4, 5], "add": [],
            "short": "rifle and bayonet dropped",
            "note": "dropped: the rifle and its bayonet (the gap to the helmet's brim, 3.6 units, would close): the helmet, the face and the shoulders, merged by the offset into one bust, the brim its widest part"},
    "mot": {"drop": [], "add": ["M13.9 3.87H15.1V9.87H13.9Z"],
            "short": "box and cab merged",
            "note": "merged: the box and the cab (the gap between them, 1 unit, would close)"},
    "mtn": {"drop": [1], "add": [],
            "short": "flag dropped",
            "note": "dropped: the flag and its pole (the gap under the flag would close in a lump on the peak): the two peaks"},
    "para": {"drop": [0, 2], "add": ["M2.92 6.18C2.92-2.06 21.08-2.06 21.08 6.18L21.08 5.335H2.92Z"],
             "short": "middle cord and hem scallops dropped",
            "note": "dropped: the middle cord, and the hem's scallops (the hem runs straight at their crests), so that the one counter between the canopy and the cords stays open"},
    "ss": {"drop": [], "add": [], "short": "offset only",
            "note": "none: offset only"},
    "tp": {"drop": [1, 2], "add": ["M6.0 3.3h2.21v3.46h-2.21Z", "M16.27 3.3h2.21v3.46h-2.21Z"],
           "short": "both funnels kept, set apart",
           "note": "redrawn: both funnels kept, set 8 units apart (from 2.5, which would close) toward the deckhouse's ends, so the gap stays open: two funnels, which the warship has not (dropped or merged, the ship read as the warship's hull at 20 px)"},
}


def skeleton(key: str, source: str, edit: dict | None = None) -> list[np.ndarray]:
    """The sign's path with its drops and merges, in its own units."""
    edit = EDITS[key] if edit is None else edit
    polys = [p for i, p in enumerate(parse_path(source)) if i not in edit.get("drop", [])]
    for extra in edit.get("add", []):
        polys.extend(parse_path(extra))
    return polys


# ---- The run -------------------------------------------------------------------------------------------------------

BEGIN = "// BEGIN seal_signs.py"
END = "// END seal_signs.py"
SUMS_BEGIN = "// BEGIN seal_signs.py sums"
SUMS_END = "// END seal_signs.py sums"


def load_alpha(path: Path) -> np.ndarray:
    image = Image.open(path)
    if image.mode != "RGBA" or image.size != (W, H):
        sys.exit(f"seal_signs: {path} is {image.mode} {image.size[0]}x{image.size[1]}, not RGBA {W}x{H}")
    return np.array(image)[..., 3]


def where_limited(alpha: np.ndarray, gap: bool, measured: float) -> tuple[float, float] | None:
    """Where a disc a step wider than the measure first takes more than a sliver: the middle of what it takes, in px."""
    n = int(round(measured * 8)) + 1
    if n > NMAX:
        return None
    ink = upsample(alpha)
    x = ~ink if gap else ink
    depth2 = sqdist_to(~x, NMAX // 4 + 2)
    e = x & (16 * depth2 >= n * n)
    removed = x & ~(16 * sqdist_to(e, n // 4 + 1) <= n * n) & (depth2 > DEPTH2)
    ys, xs = np.nonzero(removed)
    if len(xs) == 0:
        return None
    return (float(xs.mean()) / SS - PAD, float(ys.mean()) / SS - PAD)


def source_check(key: str, source: str) -> float:
    """How closely the path drawn plain at 8 px a unit matches the badge's own picture (IoU of their ink): the path
    is the one the picture was drawn from."""
    a, b = segments_of([p * UNIT for p in parse_path(source)])
    gx = (np.arange(W * SS) + 0.5) / SS
    gy = (np.arange(H * SS) + 0.5) / SS
    drawn = winding(a, b, gx, gy).reshape(H, SS, W, SS).mean(axis=(1, 3))
    old = np.array(Image.open(ORDERS / f"order-{key}.png").convert("RGBA"))[..., 3] / 255.0
    return float(np.minimum(drawn, old).sum() / np.maximum(drawn, old).sum())


def decimal(v: float) -> str:
    text = f"{v:.3f}".rstrip("0")
    return text + "0" if text.endswith(".") else text


def table_rows(rows: list[tuple[str, float, float]]) -> list[str]:
    return [f'\t\t\t\t{{ "{key}", {decimal(stroke)}, {decimal(gap)} }}, // {EDITS[key]["short"]}' for key, stroke, gap in rows]


def alpha_sum(alpha: np.ndarray) -> int:
    """The picture's alpha, row by row, as CRC-32 (zlib's, the PNG one): what edge_seals_test.cpp sums from its own
    decoding of the PNG, so it does not depend on how the file was compressed."""
    return zlib.crc32(np.ascontiguousarray(alpha, dtype=np.uint8).tobytes()) & 0xFFFFFFFF


def sum_rows(sums: list[tuple[str, int]]) -> list[str]:
    return [f'\t\t{{ "{key}", 0x{value:08x}u }},' for key, value in sums]


def read_table() -> dict[str, tuple[float, float]]:
    text = EDGE_SEALS_CPP.read_text(encoding="utf-8")
    block = text[text.index(BEGIN):text.index(END)]
    return {k: (float(s), float(g)) for k, s, g in re.findall(r'\{ "(\w+)", ([\d.]+), ([\d.]+) \}', block)}


def read_sums() -> dict[str, int]:
    text = EDGE_SEALS_TEST.read_text(encoding="utf-8")
    block = text[text.index(SUMS_BEGIN):text.index(SUMS_END)]
    return {k: int(v, 16) for k, v in re.findall(r'\{ "(\w+)", 0x([0-9a-f]{8})u \}', block)}


def write_block(path: Path, begin: str, end: str, rows: list[str]) -> None:
    """The rows between the markers, inside the array the begin line's comment introduces: everything up to its
    opening brace and from its closing one is kept."""
    lines = path.read_text(encoding="utf-8").split("\n")
    # (a marker is the whole line, or the line's start followed by its comment in brackets: "// BEGIN seal_signs.py"
    # is not "// BEGIN seal_signs.py sums")
    marker = lambda line, mark: re.match(re.escape(mark) + r"(\s*$|\s+\()", line.strip()) is not None
    first = next(i for i, line in enumerate(lines) if marker(line, begin))
    last = next(i for i in range(first, len(lines)) if marker(lines[i], end))
    head = next(i for i in range(first, last) if lines[i].rstrip().endswith("= {"))
    tail = next(i for i in range(last, first, -1) if lines[i].strip() == "};")
    lines[head + 1:tail] = rows
    path.write_text("\n".join(lines), encoding="utf-8")


def write_table(rows: list[tuple[str, float, float]], sums: list[tuple[str, int]]) -> None:
    write_block(EDGE_SEALS_CPP, BEGIN, END, table_rows(rows))
    write_block(EDGE_SEALS_TEST, SUMS_BEGIN, SUMS_END, sum_rows(sums))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--write-table", action="store_true",
                        help="write the measures into EdgeSeals.cpp and the pictures' sums into edge_seals_test.cpp")
    parser.add_argument("--check", action="store_true",
                        help="draw nothing: measure and sum the pictures and compare the table and the sums")
    args = parser.parse_args()
    sources = read_sources()
    rows: list[tuple[str, float, float]] = []
    sums: list[tuple[str, int]] = []
    failed = False
    for key in PRESSABLE:
        out = SEALS / f"seal-{key}.png"
        if args.check:
            alpha = load_alpha(out)
            share = float("nan")
        else:
            likeness = source_check(key, sources[key])
            if likeness < 0.98:
                print(f"{key}: the path does not match order-{key}.png (IoU {likeness:.3f})")
                failed = True
            polys, share = place(skeleton(key, sources[key]), parse_path(sources[key]))
            alpha, _ = draw(polys)
            save_png(alpha, out)
        stroke, gap = measure(alpha, False), measure(alpha, True)
        rows.append((key, stroke, gap))
        sums.append((key, alpha_sum(alpha)))
        at_s, at_g = where_limited(alpha, False, stroke), where_limited(alpha, True, gap)
        spot = lambda at: "-" if at is None else f"({at[0]:.0f},{at[1]:.0f})"
        drawn = "  -  " if math.isnan(share) else f"{share:.3f}"
        print(f"{key:5s} drawn at {drawn}  stroke {stroke:6.3f} at {spot(at_s):9s}  gap {gap:6.3f} at {spot(at_g):9s}  sum {sums[-1][1]:08x}  {EDITS[key]['short']}")
        if min(stroke, gap) < FLOOR - 0.5:
            print(f"{key}: under the floor of {FLOOR} px (with 0.5 for the antialiasing)")
            failed = True
    for key in REFUSED:
        if (SEALS / f"seal-{key}.png").exists():
            print(f"{key}: has a seal picture, but is never pressed")
            failed = True
    if args.check:
        table = read_table()
        for key, stroke, gap in rows:
            if key not in table or abs(table[key][0] - stroke) > 0.25 or abs(table[key][1] - gap) > 0.25:
                print(f"{key}: the table has {table.get(key)}, the picture measures ({stroke}, {gap})")
                failed = True
        if set(table) != set(PRESSABLE):
            print(f"the table's signs {sorted(table)} are not the pressable ones")
            failed = True
        held = read_sums()
        for key, value in sums:
            if held.get(key) != value:
                print(f"{key}: the picture's sum is {value:08x}, edge_seals_test.cpp has {held.get(key, 0):08x}: the picture is not the one drawn")
                failed = True
        if set(held) != set(PRESSABLE):
            print(f"the sums' signs {sorted(held)} are not the pressable ones")
            failed = True
    elif args.write_table:
        write_table(rows, sums)
        print(f"wrote {len(rows)} measures into {EDGE_SEALS_CPP} and {len(sums)} sums into {EDGE_SEALS_TEST}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
