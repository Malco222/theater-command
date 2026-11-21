"""Nose art for the bombers (Brian, 2026-11-20: "have some bomber nose art on the b-17s"; then "make 100 bomber names
and nose art so its not always the same ones each mission" and "lets actually make it 200").

The 200 names, motifs, lettering, themes and kinds are the air thread's list (noseart_names.json beside this file, a
copy of /mnt/project-files/air-battles/nose-art/names.json; 1 to 24 checked by the historian, 25 to 200 with him).
The look is the art director's (2026-11-20, G3 of art-director/battles/air-fates-look.md): flat sign-painter's
cartoons, a dark outline that varies in thickness as a brush gives it, three or four colours, slightly faded house
paint with chips showing the olive drab; never a clean vector print, a gradient or a glow. Every figure fully dressed,
in the clothes of the time. Every painting is drawn here, by us, from flat shapes: nothing is copied from any real
aircraft's art, and no name or picture is a real person's, a real company's or any story's character.

Each painting is a decal: a picture with a clear background, PX pixels for SIZE_M metres of her skin (the painting
fitted inside, its chips clear so that her own olive drab shows through them):

    decal(n)                -> the picture (a PIL image, RGBA), or None if painting n is not drawn yet
    write(folder, ids=None) -> art-NNN.png for every painting drawn, and done.json / done.txt: the numbers done
    preview(n, path)        -> the painting on olive drab, to look at
    DONE                    -> the numbers whose painting exists (the scene draws only from these)

A painting is drawn on a grid about 100 wide (y up) with:
  fill(paint, points)          a closed shape through its points (smoothed), with the painter's dark edge round it
  shape(paint, points)         the same with straight sides
  oval(paint, x, y, rx, ry)    an ellipse
  blobs(paint, circles)        lumps run together (a cloud, smoke, a bush): one edge round them all
  line(paint, points, w)       a brush stroke along its points, w wide (thinner at its ends)
  name(text, lettering, ...)   the name, in the sign painter's hand the list gives: script, block or stencil
and the kit below: lady() (a woman of the 1940s, fully dressed, in any of a dozen outfits and poses), bomb(), star(),
bolt(), wing(), cloud and smoke, and the beasts and things the list asks for.
"""
import json
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import lettering

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "noseart_names.json")) as _fh:
    LIST = json.load(_fh)
ART = LIST["art"]
BY_ID = {a["id"]: a for a in ART}

SIZE_M = (2.2, 1.35)             # (the decal's own size on her skin, metres: the painting fits inside it, its name's capitals 20 to 30 cm)
PX = (1408, 864)                 # (its pixels: 640 to the metre, enough for a close look at her name)
PX_PER_M = PX[0] / SIZE_M[0]

# House paint, as a crew chief's shelf had it in 1943: nothing bright, nothing pure.
PAINTS = {
    "art_ink": "1E1B18", "art_white": "D8D3C3", "art_cream": "D9C9A0", "art_red": "A33A2E", "art_blue": "3F5A85",
    "art_sky": "93AFC8", "art_yellow": "D0A23C", "art_green": "557A3E", "art_lgreen": "8FA86A", "art_brown": "75543A",
    "art_tan": "B08C5E", "art_grey": "8C8E8E", "art_dark": "3E4246", "art_navy": "28334D", "art_pink": "C98E86",
    "art_orange": "C26F34", "art_hair": "5B3A26", "art_blond": "C7A35A", "art_auburn": "86452B", "art_black": "2B2624",
    "art_skin": "D9AE88", "art_skin2": "B4835C", "art_skin3": "84583D", "art_steel": "9AA1A7", "art_lsteel": "C6CACB",
    "art_olive": "6A6845", "art_khaki": "A8996C", "art_violet": "5D62A6", "art_lred": "C4645A", "art_gold": "B88A2E",
    "art_lgrey": "B5B6B2", "art_dgreen": "3D5531", "art_lblue": "6F8DB5", "art_lbrown": "9A7550",
}
COLOUR = {"yellow": "art_yellow", "white": "art_white", "cream": "art_cream", "red": "art_red", "blue": "art_blue",
          "pale blue": "art_sky", "green": "art_green", "light green": "art_lgreen", "orange": "art_orange", "black": "art_ink"}


def rgb(paint):
    h = PAINTS.get(paint, paint if len(paint) == 6 else "FF00FF")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


# ---- curves ---------------------------------------------------------------------------------------------------------
def smooth(points, closed=True, steps=6):
    """A Catmull-Rom curve through the points."""
    p = [np.array(q, dtype=float) for q in points]
    n = len(p)
    if n < 3:
        return [tuple(q) for q in p]
    out = []
    rng = range(n) if closed else range(n - 1)
    for i in rng:
        p0 = p[(i - 1) % n] if closed else p[max(i - 1, 0)]
        p1, p2 = p[i], p[(i + 1) % n]
        p3 = p[(i + 2) % n] if closed else p[min(i + 2, n - 1)]
        for k in range(steps):
            t = k / steps
            t2, t3 = t * t, t * t * t
            q = 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)
            out.append((float(q[0]), float(q[1])))
    if not closed:
        out.append((float(p[-1][0]), float(p[-1][1])))
    return out


def area(pts):
    return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))


def ccw(pts):
    return pts if area(pts) > 0 else list(reversed(pts))


def ellipse(x, y, rx, ry, turn=0.0, n=36, a0=0.0, a1=360.0):
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    full = abs(a1 - a0) >= 359.9
    m = n if full else n + 1
    out = []
    for k in range(m):
        a = math.radians(a0 + (a1 - a0) * k / (n if not full else n))
        u, v = rx * math.cos(a), ry * math.sin(a)
        out.append((x + u * c - v * s, y + u * s + v * c))
    return out


def lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def turned(pts, cx, cy, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def limb(pts, radii, steps=5):
    """The outline of a limb (or a tail, a stalk, a rope) along `pts`, `radii` wide at each: round at both ends."""
    c = smooth(pts, False, steps) if len(pts) > 2 else [lerp(pts[0], pts[1], t / 8.0) for t in range(9)]
    n, m = len(pts), len(c)
    rr = []
    for i in range(m):
        u = i * (n - 1) / max(m - 1, 1)
        j = min(int(u), n - 2)
        f = u - j
        rr.append(radii[j] * (1 - f) + radii[j + 1] * f)
    left, right, tang = [], [], []
    for i in range(m):
        a, b = np.array(c[max(i - 1, 0)]), np.array(c[min(i + 1, m - 1)])
        t = b - a
        ln = np.hypot(*t)
        t = t / ln if ln > 1e-9 else np.array([1.0, 0.0])
        nrm = np.array([-t[1], t[0]])
        p = np.array(c[i])
        left.append(p + nrm * rr[i])
        right.append(p - nrm * rr[i])
        tang.append(t)

    def untangle(side):
        keep = [side[0]]
        for i in range(1, m):
            if np.dot(side[i] - keep[-1], tang[i]) > -1e-6:
                keep.append(side[i])
        return keep

    left, right = untangle(left), untangle(right)
    t0, t1 = tang[0], tang[-1]
    a_end = math.atan2(t1[0], -t1[1])                       # (from the left side round the far end to the right)
    end = [np.array(c[-1]) + rr[-1] * np.array([math.cos(a_end - math.pi * k / 8), math.sin(a_end - math.pi * k / 8)]) for k in range(1, 8)]
    a_start = math.atan2(-t0[0], t0[1])                     # (from the right side round the near end to the left)
    start = [np.array(c[0]) + rr[0] * np.array([math.cos(a_start - math.pi * k / 8), math.sin(a_start - math.pi * k / 8)]) for k in range(1, 8)]
    ring = left + end + list(reversed(right)) + start
    return [(float(p[0]), float(p[1])) for p in ring]


# ---- the drawing ----------------------------------------------------------------------------------------------------
class Drawing:
    """One painting, on a grid about 100 wide, built up from its lowest layer."""

    def __init__(self, seed, ink=0.62, crude=False):
        self.ops = []                                    # [("polys", paint, [poly, ...]) | ("text", {...})]
        self.rng = random.Random(seed)
        self.seed = seed
        self.crude = crude                               # (by one of the cruder hands: see crude())
        self.ink = ink                                   # (how thick the painter's edge is, on the grid)

    def _edge(self, pts, w, closed=True):
        """The painter's dark edge round a shape, never even: heavier on its shadow side (below and to the right),
        where the brush pressed, thinner over the top; now and then it runs a little past a sharp corner, and now and
        then the brush skipped and left a gap (the art director, 2026-11-20 17:44Z)."""
        if w <= 0.0 or len(pts) < 3:
            return []
        pts = ccw(pts) if closed else pts
        n = len(pts)
        ph = self.rng.uniform(0.0, 6.28)
        fr = self.rng.uniform(5.0, 11.0)
        sway = 0.42 if self.crude else 0.28
        ring_in, ring_out = [], []
        for i in range(n):
            a, b, c = np.array(pts[(i - 1) % n]), np.array(pts[i]), np.array(pts[(i + 1) % n])
            if not closed and i == 0:
                a = b
            if not closed and i == n - 1:
                c = b
            t = c - a
            ln = np.hypot(*t)
            t = t / ln if ln > 1e-9 else np.array([1.0, 0.0])
            nrm = np.array([t[1], -t[0]])                # (outward on a counter-clockwise shape)
            shadow = max(0.0, 0.55 * nrm[0] - 0.83 * nrm[1])
            press = 0.6 + sway * math.sin(ph + fr * i / n) + 0.8 * shadow
            ww = w * max(press, 0.35)
            ring_in.append(b - nrm * ww * 0.45)
            ring_out.append(b + nrm * ww * 0.55)
        m = n if closed else n - 1
        skip = set()
        if closed and n >= 16 and self.rng.random() < (0.45 if self.crude else 0.3):
            run = max(1, int(n * self.rng.uniform(0.012, 0.03)))
            s0 = self.rng.randrange(n)
            skip = {(s0 + k) % n for k in range(run)}
        quads = []
        for i in range(m):
            if i in skip:
                continue
            j = (i + 1) % n
            quads.append([tuple(ring_in[i]), tuple(ring_out[i]), tuple(ring_out[j]), tuple(ring_in[j])])
        for i in range(n):                                # (past a sharp corner, now and then)
            if not closed and (i == 0 or i == n - 1):
                continue
            a, b, c = np.array(pts[(i - 1) % n]), np.array(pts[i]), np.array(pts[(i + 1) % n])
            t1, t2 = b - a, c - b
            l1, l2 = np.hypot(*t1), np.hypot(*t2)
            if l1 < 1e-9 or l2 < 1e-9:
                continue
            t1, t2 = t1 / l1, t2 / l2
            if float(t1 @ t2) < 0.57 and self.rng.random() < 0.5:
                tip = b + t1 * w * self.rng.uniform(0.6, 1.3)
                nn = np.array([t1[1], -t1[0]]) * w * 0.24
                quads.append([tuple(b + nn), tuple(tip + nn * 0.4), tuple(tip - nn * 0.4), tuple(b - nn)])
        return quads

    def _put(self, paint, polys):
        self.ops.append(("polys", paint, [[(float(u), float(v)) for u, v in p] for p in polys if len(p) >= 3]))

    def _inked(self, paint, pts, ink, closed=True):
        if paint:
            self._put(paint, [pts])
        w = self.ink if ink is None else ink
        if w:
            self._put("art_ink", self._edge(pts, w, closed))
        return pts

    def fill(self, paint, points, ink=None, steps=6):
        return self._inked(paint, ccw(smooth(points, True, steps)), ink)

    def shape(self, paint, points, ink=None):
        return self._inked(paint, ccw([tuple(p) for p in points]), ink)

    def oval(self, paint, x, y, rx, ry, turn=0.0, ink=None, n=36):
        return self._inked(paint, ccw(ellipse(x, y, rx, ry, turn, n)), ink)

    def limb(self, paint, pts, radii, ink=None):
        return self._inked(paint, ccw(limb(pts, radii)), ink)

    def blobs(self, paint, circles, ink=None, back=None):
        """Lumps run together, one edge round them all: every lump's ink first (a little bigger), then every fill."""
        w = self.ink if ink is None else ink
        if w:
            self._put("art_ink", [ellipse(x, y, r + w * (0.5 + 0.25 * self.rng.random()), (r if ry is None else ry) + w * (0.5 + 0.25 * self.rng.random()))
                                  for x, y, r, ry in [(c + (None,))[:4] for c in circles]])
        self._put(paint, [ellipse(x, y, r, r if ry is None else ry) for x, y, r, ry in [(c + (None,))[:4] for c in circles]])
        if back:
            self._put(back, [ellipse(x + r * 0.18, y - r * 0.2, r * 0.72, (r if ry is None else ry) * 0.62) for x, y, r, ry in [(c + (None,))[:4] for c in circles]])

    def line(self, paint, points, w, steps=5, taper=True, closed=False):
        """A brush stroke: w wide in its middle, thinning to its ends if `taper`."""
        pts = smooth(points, closed, steps) if len(points) > 2 else [tuple(p) for p in points]
        if closed:
            pts = pts + [pts[0]]
        n = len(pts)
        left, right = [], []
        for i in range(n):
            a, c = np.array(pts[max(i - 1, 0)]), np.array(pts[min(i + 1, n - 1)])
            t = c - a
            ln = np.hypot(*t)
            t = t / ln if ln > 1e-9 else np.array([1.0, 0.0])
            nrm = np.array([-t[1], t[0]])
            f = math.sin(math.pi * (0.1 + 0.8 * i / max(n - 1, 1))) if taper else 1.0
            ww = 0.5 * w * max(f, 0.25)
            b = np.array(pts[i])
            left.append(b + nrm * ww)
            right.append(b - nrm * ww)
        quads = []
        for i in range(n - 1):
            quads.append([tuple(left[i]), tuple(right[i]), tuple(right[i + 1]), tuple(left[i + 1])])
        self._put(paint, quads)

    def name(self, text, lettering, at, size, turn=0.0, wave=0.5, bend=0.0, jitter=1.0):
        """The name in the painter's hand (`lettering` as the list has it: "yellow script, black edge"...), its middle
        at `at`, its capitals `size` high on the grid, turned `turn` degrees; `bend` lifts its ends (grid units) so
        that it arches; a little uneven, as a brush leaves it."""
        main, *rest = [w.strip() for w in lettering.split(",")]
        bits = main.split()
        hand = bits[-1]
        colour = COLOUR[" ".join(bits[:-1])]
        edge = shadow = None
        for r in rest:
            words = r.split()
            if words[-1] == "edge":
                edge = COLOUR[" ".join(words[:-1])]
            if words[-1] == "shadow":
                shadow = COLOUR[" ".join(words[:-1])]
        if edge is None and hand != "stencil" and not (hand == "block" and shadow):
            edge = "art_ink"                             # (a thin dark edge, as most painters gave a name on olive drab)
        outer = rgb(edge or colour)                      # (a pale name with no dark edge gets a thin dark keyline round it, so
        keyline = "art_ink" if 0.299 * outer[0] + 0.587 * outer[1] + 0.114 * outer[2] >= 120 else None   # it holds on her grey)
        self.ops.append(("text", {"text": text.upper() if hand in ("block", "stencil") else text, "hand": hand, "fill": colour,
                                  "edge": edge, "shadow": shadow, "keyline": keyline, "at": at, "size": size, "turn": turn, "wave": wave,
                                  "bend": bend, "jitter": jitter, "seed": self.rng.randint(0, 10 ** 6), "crude": self.crude}))


# ---- the sign painter's hand ------------------------------------------------------------------------------------------
# Every name is lettered by hand (lettering.py: our own brush alphabet, never a typeface), each by a hand of its own
# (its seed), and about one painting in four by a cruder hand (`crude`), the art director's mend of 2026-11-20 17:44Z.
def crude(n):
    """Whether painting n is by one of the cruder hands (about one in four: wobblier, fewer colours, plainer letters)."""
    return n is not None and random.Random(n * 7919 + 11).random() < 0.25


def hand_of(t):
    return lettering.Hand(t["hand"], t["seed"], t.get("crude", False))


def text_bounds(t):
    """Roughly where a name lies on the grid (for fitting the painting to its decal)."""
    w = lettering.measure(t["text"], hand_of(t)) * t["size"] + t["size"] * 0.3
    h = t["size"] * 1.75 + abs(t["bend"])
    cx, cy = t["at"]
    c, s = math.cos(math.radians(t["turn"])), math.sin(math.radians(t["turn"]))
    pts = [(cx + u * c - v * s, cy + u * s + v * c) for u in (-w / 2, w / 2) for v in (-h / 2, h / 2)]
    return min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)


def smooth_noise(rng, shape, cell):
    """Noise from -1 to 1, smooth over `cell` (pixels down, across)."""
    h, w = shape
    gh, gw = max(2, int(h / cell[0]) + 2), max(2, int(w / cell[1]) + 2)
    g = rng.random((gh, gw)).astype(np.float32)
    im = Image.fromarray((g * 255).astype(np.uint8), "L").resize((w, h), Image.BICUBIC)
    return np.asarray(im).astype(np.float32) / 127.5 - 1.0


def draw_text(canvas, t, k, ox, oy):
    """The name onto the canvas (pixels: x = ox + k u, y = oy - k v), lettered by its painter's hand."""
    size_px = t["size"] * k
    im = lettering.letter(t["text"], hand_of(t), size_px, rgb(t["fill"]), rgb(t["edge"]) if t["edge"] else None,
                          rgb(t["shadow"]) if t["shadow"] else None, t["wave"], t["bend"] / max(t["size"], 1e-6),
                          rgb(t["keyline"]) if t.get("keyline") else None)
    if t["turn"]:
        im = im.rotate(t["turn"], resample=Image.BICUBIC, expand=True)
    cx, cy = ox + k * t["at"][0], oy - k * t["at"][1]
    canvas.alpha_composite(im, (int(cx - im.size[0] / 2), int(cy - im.size[1] / 2)))


# ---- the decal --------------------------------------------------------------------------------------------------------
KEEP = {"art_ink", "art_skin", "art_skin2", "art_skin3", "art_hair", "art_blond", "art_auburn", "art_black"}
BARE = {"art_olive"}                                     # (a cruder hand leaves her own olive drab for these)
TINS = {"art_red", "art_yellow", "art_white"}            # (the flight line's commonest tins, the first a cruder hand reaches for)


def _dense(poly, step=1.2):
    """The polygon with no side longer than `step` (grid units), so that a shaky hand can bend its long sides."""
    out = []
    for i, (u0, v0) in enumerate(poly):
        u1, v1 = poly[(i + 1) % len(poly)]
        k = max(1, int(math.hypot(u1 - u0, v1 - v0) / step))
        out += [(u0 + (u1 - u0) * j / k, v0 + (v1 - v0) * j / k) for j in range(k)]
    return out


def cruder(d):
    """A cruder hand's painting (the art director: "wobblier, fewer colours, plainer letters. Many hands painted
    these"): its outlines and fills sway together off their true line, broadly and in a small shake; it keeps only
    three colours of its own (its biggest, the commonest tins first) besides its skin, hair and ink, and every other
    paint becomes the nearest of those; and where it would have painted olive drab it leaves her own. Its name keeps
    its colours, in the cruder letters its hand already gives it."""
    r = random.Random(d.seed * 3 + 1)
    waves = [(r.uniform(0.05, 0.11), r.uniform(0.05, 0.11), r.uniform(0.0, 6.28)) for _ in range(4)]
    shake = [(r.uniform(0.6, 1.2), r.uniform(0.0, 6.28)) for _ in range(4)]
    amp, amp2 = 0.55, 0.2

    def sway(u, v):
        du = amp * (math.sin(waves[0][0] * v + waves[0][2]) + 0.6 * math.sin(waves[1][1] * (u + v) + waves[1][2])) + \
            amp2 * (math.sin(shake[0][0] * v + shake[0][1]) + 0.5 * math.sin(shake[1][0] * (u + v) + shake[1][1]))
        dv = amp * (math.sin(waves[2][0] * u + waves[2][2]) + 0.6 * math.sin(waves[3][1] * (u - v) + waves[3][2])) + \
            amp2 * (math.sin(shake[2][0] * u + shake[2][1]) + 0.5 * math.sin(shake[3][0] * (u - v) + shake[3][1]))
        return u + du, v + dv

    room = {}
    for op in d.ops:
        if op[0] == "polys" and op[1] not in KEEP and op[1] not in BARE:
            room[op[1]] = room.get(op[1], 0.0) + sum(abs(area(p)) for p in op[2]) * (2.0 if op[1] in TINS else 1.0)
    kept = sorted(room, key=room.get, reverse=True)[:3]

    def nearest(paint):
        if paint in BARE:
            return "bare"
        if paint in KEEP or paint in kept or not kept:
            return paint
        c = rgb(paint)
        return min(kept, key=lambda k: sum((x - y) ** 2 for x, y in zip(rgb(k), c)))

    ops = []
    for op in d.ops:
        if op[0] == "polys":
            ops.append(("polys", nearest(op[1]), [[sway(u, v) for u, v in _dense(poly)] for poly in op[2]]))
        else:
            ops.append(op)
    d.ops = ops
    return d


def raster(d, px=PX, ss=3, margin=0.035, caps_cm=None):
    """The painting drawn into its decal, fitted inside it with a little room round it; or, with `caps_cm` (a name
    alone), drawn so that its capitals are that tall on her skin, in the middle of the decal (smaller only if it
    would not fit)."""
    if getattr(d, "crude", False) and not getattr(d, "crudened", False):
        cruder(d)
        d.crudened = True
    W, H = px[0] * ss, px[1] * ss
    xs, ys = [], []
    for op in d.ops:
        if op[0] == "polys":
            for poly in op[2]:
                xs += [p[0] for p in poly]
                ys += [p[1] for p in poly]
        else:
            x0, y0, x1, y1 = text_bounds(op[1])
            xs += [x0, x1]
            ys += [y0, y1]
    per_m = px[0] / SIZE_M[0]                            # (the name's capitals, in centimetres on her skin: the art director asks 20 to 30)
    grown = 1.0
    for _ in range(5):
        xs, ys = [], []
        for op in d.ops:
            if op[0] == "polys":
                for poly in op[2]:
                    xs += [p[0] for p in poly]
                    ys += [p[1] for p in poly]
            else:
                x0, y0, x1, y1 = text_bounds(op[1])
                xs += [x0, x1]
                ys += [y0, y1]
        gx0, gx1, gy0, gy1 = min(xs), max(xs), min(ys), max(ys)
        k = min(W * (1 - 2 * margin) / (gx1 - gx0), H * (1 - 2 * margin) / (gy1 - gy0))
        if caps_cm:
            sizes = [op[1]["size"] for op in d.ops if op[0] == "text"]
            k = min(k, caps_cm * ss * per_m / (100.0 * max(sizes)))
        caps = [100 * op[1]["size"] * k / ss / per_m for op in d.ops if op[0] == "text"]
        if caps_cm or not caps or min(caps) >= 20.3 or grown >= 1.14:
            break
        f = min(20.6 / min(caps), 1.14 / grown)          # (a name a little short of 20 cm grows where it stands)
        grown *= f
        for op in d.ops:
            if op[0] == "text":
                op[1]["size"] *= f
    LAST_CAPS[getattr(d, "n", None)] = [round(c, 1) for c in caps]
    ox, oy = W / 2 - k * (gx0 + gx1) / 2, H / 2 + k * (gy0 + gy1) / 2
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    for op in d.ops:
        if op[0] == "polys":
            col = (0, 0, 0, 0) if op[1] == "bare" else rgb(op[1]) + (255,)     # ("bare": her own olive drab, left unpainted)
            for poly in op[2]:
                dr.polygon([(ox + k * u, oy - k * v) for u, v in poly], fill=col)
        else:
            draw_text(im, op[1], k, ox, oy)
            dr = ImageDraw.Draw(im)
    return im.resize(px, Image.LANCZOS)


_SKIN = {}


def skin_under():
    """Her seams and rivets under the picture (noseart_skin.json, made by noseart_skin.py from her model)."""
    if not _SKIN:
        path = os.path.join(HERE, "noseart_skin.json")
        _SKIN.update(json.load(open(path)) if os.path.exists(path) else {"seams": [], "rivets": []})
    return _SKIN


def _blur(a, r):
    im = Image.fromarray(np.clip(a * 255.0, 0, 255).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(max(r, 0.5)))
    return np.asarray(im).astype(np.float32) / 255.0


def _shift(a, dx, dy):
    """The array moved dx pixels right and dy down (its edges repeated)."""
    h, w = a.shape
    ys = np.clip(np.arange(h) - dy, 0, h - 1)
    xs = np.clip(np.arange(w) - dx, 0, w - 1)
    return a[ys][:, xs]


def _gauss_noise(rng, shape, cell):
    """Smooth noise of unit spread over `cell` pixels, never clipped (so that it ranks cleanly)."""
    h, w = shape
    g = rng.standard_normal((max(2, int(h / cell) + 2), max(2, int(w / cell) + 2))).astype(np.float32)
    a = np.asarray(Image.fromarray(g, "F").resize((w, h), Image.BICUBIC))
    return a / max(float(a.std()), 1e-6)


def weather(im, seed, fade=0.1, chips=1.0):
    """Slightly faded house paint, brushed on over her skin (the art director, 2026-11-20 17:44Z):
    - her skin shows through: her seams and rivets press through the paint, and the paint takes her sheen, her
      panels' differences and her dirt, streaked aft;
    - it wears where hands, boots and the wind wear it: chips at her seams and rivet rows and along the forward edges
      of its shapes, heaviest along the top and the lower front, and a few scrapes; never an even speckle.
    The chips and scrapes are clear, so that her olive drab shows through them. The picture is her port side's: its
    left is forward."""
    rng = np.random.default_rng(seed)
    r = random.Random(seed)
    a = np.asarray(im).astype(np.float32) / 255.0
    col, al = a[..., :3].copy(), a[..., 3].copy()
    h, w = al.shape
    per_m = w / SIZE_M[0]
    U = np.broadcast_to(np.linspace(0.0, 1.0, w, dtype=np.float32)[None, :], (h, w))
    V = np.broadcast_to(np.linspace(1.0, 0.0, h, dtype=np.float32)[:, None], (h, w))
    lum = col @ np.array([0.3, 0.59, 0.11], dtype=np.float32)
    col = col * 0.88 + lum[..., None] * 0.12                                   # (the sun takes the colour out)
    col = col * (1 - fade) + np.array([0.80, 0.78, 0.70], dtype=np.float32) * fade
    streak = smooth_noise(rng, (h, w), (3, 40))                                # (the brush's strokes, along the painting)
    blot = smooth_noise(rng, (h, w), (60, 60))
    col *= (1.0 + 0.025 * streak + 0.025 * blot)[..., None]

    # (her skin under it: seams, panel lines and rivets, where her model has them)
    sk = skin_under()
    lines = Image.new("L", (w, h), 0)
    dl = ImageDraw.Draw(lines)
    lip = Image.new("L", (w, h), 0)
    dp = ImageDraw.Draw(lip)
    gap = max(2, int(round(0.0045 * per_m)))
    for u0, v0, u1, v1 in sk.get("seams", []):
        p0, p1 = (u0 * w, (1 - v0) * h), (u1 * w, (1 - v1) * h)
        dl.line([p0, p1], fill=255, width=gap)
        dp.line([(p0[0], p0[1] - gap), (p1[0], p1[1] - gap)], fill=255, width=max(1, gap // 2))
    heads = Image.new("L", (w, h), 0)
    dh = ImageDraw.Draw(heads)
    shine = Image.new("L", (w, h), 0)
    ds = ImageDraw.Draw(shine)
    rr = max(2.0, 0.0055 * per_m)
    for u, v in sk.get("rivets", []):
        x, y = u * w, (1 - v) * h
        dh.ellipse([x - rr, y - rr, x + rr, y + rr], fill=255)
        ds.ellipse([x - rr * 0.75, y - rr * 0.85, x + rr * 0.05, y - rr * 0.05], fill=255)
    seam = np.asarray(lines).astype(np.float32) / 255.0
    seam_lip = np.asarray(lip).astype(np.float32) / 255.0 * (1.0 - seam)
    head = _blur(np.asarray(heads).astype(np.float32) / 255.0, 0.8)
    head_shine = _blur(np.asarray(shine).astype(np.float32) / 255.0, 0.6)

    # (her panels, each faded a little its own way, as her coat is; her sheen, lighter high up; her dirt, streaked aft
    #  in the wind and heaviest low down)
    panels = Image.fromarray(np.where(seam > 0.2, 255, 0).astype(np.uint8), "L")
    label = 1
    for gy in range(h // 16, h, h // 8):
        for gx in range(w // 24, w, w // 12):
            if panels.getpixel((gx, gy)) == 0 and label < 250:
                ImageDraw.floodfill(panels, (gx, gy), label)
                label += 1
    shade = np.array([1.0] + [1.0 + r.uniform(-0.03, 0.03) for _ in range(254)] + [1.0], dtype=np.float32)
    col *= shade[np.asarray(panels)][..., None]
    col *= (1.0 + 0.035 * (V - 0.5))[..., None]
    aft = smooth_noise(rng, (h, w), (7, 220))
    low = V[..., :] * 0.0 + (1.0 - V) ** 3
    col *= (1.0 - 0.05 * np.clip(aft, 0, 1) - 0.07 * low * (0.6 + 0.4 * blot))[..., None]

    # (pressed through the paint: a dark line where two skins meet, and the lap's edge catching the light above it; a
    #  rivet a small dark dome with a light point on it)
    col *= (1.0 - 0.3 * seam)[..., None]
    col *= (1.0 + 0.08 * seam_lip)[..., None]
    col *= (1.0 - 0.16 * head + 0.14 * head_shine)[..., None]

    # (where it wears: along her seams and rivet rows, along the forward edges of its shapes, heaviest along the top and
    #  the lower front)
    near_seam = np.clip(_blur(seam, 0.012 * per_m) * 5.0, 0, 1)
    near_rivet = np.clip(_blur(head, 0.008 * per_m) * 4.0, 0, 1)
    d1 = max(2, int(0.006 * per_m))
    fwd = np.clip(al - _shift(al, d1, 0), 0, 1)                               # (paint here, none just ahead of it)
    lumc = col @ np.array([0.3, 0.59, 0.11], dtype=np.float32)
    fwd_in = np.clip(np.abs(lumc - _shift(lumc, d1, 0)) * 4.0, 0, 1) * al * 0.5   # (and where one colour meets the next)
    fwd = np.clip(_blur(np.maximum(fwd, fwd_in), 0.006 * per_m) * 3.0, 0, 1)
    edge = np.clip((0.96 - _blur(al, 0.01 * per_m)) * 3.0, 0, 1) * (al > 0.5)
    rows, cols = np.nonzero(al > 0.5)                                         # (the painting's own top and lower front, not the decal's)
    v_hi, v_lo = (1.0 - rows.min() / h, 1.0 - rows.max() / h) if rows.size else (1.0, 0.0)
    u_lo, u_hi = (cols.min() / w, cols.max() / w) if cols.size else (0.0, 1.0)
    tall, wide = max(v_hi - v_lo, 1e-3), max(u_hi - u_lo, 1e-3)
    top = np.clip((V - (v_hi - 0.22 * tall)) / (0.22 * tall), 0, 1) ** 1.2
    low_front = np.clip((u_lo + 0.4 * wide - U) / (0.4 * wide), 0, 1) * np.clip((v_lo + 0.4 * tall - V) / (0.4 * tall), 0, 1)
    patch = smooth_noise(rng, (h, w), (40, 40))
    P = 0.85 * near_seam + 0.6 * near_rivet + 0.75 * fwd + 0.1 * edge + 0.65 * top + 0.75 * low_front
    P = np.clip(P, 0.0, 1.4) * (0.45 + 0.75 * np.clip(patch * 0.5 + 0.5, 0, 1))   # (patchy: here it held, there it went)
    rate = np.clip((P - 0.15) / 1.15, 0.0, 1.0) ** 1.5 * 0.38 * chips              # (the share of it gone: none where nothing wears it)
    grain = _gauss_noise(rng, (h, w), 3.2) + 0.4 * _gauss_noise(rng, (h, w), 1.2)
    q = np.empty(grain.size, dtype=np.float32)
    q[np.argsort(grain, axis=None)] = np.linspace(0.0, 1.0, grain.size, dtype=np.float32)
    chip = q.reshape(h, w) > 1.0 - rate                                           # (flakes, bigger and run together where it wears most)
    al = np.where(chip, 0.0, al)

    # (a few long scrapes: boots and ladders, low and forward mostly)
    sc = Image.new("L", (w, h), 0)
    dsc = ImageDraw.Draw(sc)
    for _ in range(r.randint(2, 5)):
        x, y = r.uniform(0.02, 0.75) * w, r.uniform(0.45, 0.95) * h
        ang = math.radians(r.uniform(-25.0, 25.0) + (180.0 if r.random() < 0.3 else 0.0))
        length = r.uniform(0.06, 0.3) * per_m
        bend = r.uniform(-0.004, 0.004)
        pts = []
        for k in range(24):
            t = k / 23.0
            aa = ang + bend * length * (t - 0.5)
            pts.append((x + math.cos(aa) * length * t, y - math.sin(aa) * length * t))
        width = r.choice((1, 1, 2, 2, 3))
        for p0, p1 in zip(pts, pts[1:]):
            if r.random() < 0.88:                        # (a scrape skips where the paint held)
                dsc.line([p0, p1], fill=255, width=width)
    al = np.where(np.asarray(sc) > 0, 0.0, al)

    # (where it is clear: the nearest paint's colour a few pixels out, so that her texture filtering draws no fringe
    #  round the paint or the chips; farther out her olive drab, flat, so that the file stays small)
    pa = col * al[..., None]
    s_al = _box(al, 4)
    s_pa = np.dstack([_box(pa[..., i], 4) for i in range(3)])
    fill = np.where((s_al > 1e-3)[..., None], s_pa / np.maximum(s_al, 1e-3)[..., None], np.array(CLEAR, dtype=np.float32))
    col = np.where((al <= 0.0)[..., None], fill, col)
    out = np.dstack([np.clip(col, 0, 1), np.clip(al, 0, 1)])
    return Image.fromarray((out * 255).astype(np.uint8), "RGBA")


CLEAR = (76 / 255.0, 75 / 255.0, 53 / 255.0)             # (the colour under a decal's clear pixels: her olive drab)


def _box(a, r):
    """The mean of `a` over a square 2r+1 pixels wide round each pixel (its edges repeated)."""
    p = np.pad(a.astype(np.float64), r + 1, mode="edge")
    c = p.cumsum(0).cumsum(1)
    k = 2 * r + 1
    h, w = a.shape
    tot = c[k:k + h, k:k + w] - c[0:h, k:k + w] - c[k:k + h, 0:w] + c[0:h, 0:w]
    return (tot / (k * k)).astype(np.float32)


LAST_CAPS = {}


def drawing(n):
    if n not in PAINTINGS:
        return None
    d = Drawing(n * 7919, crude=crude(n))
    d.n = n
    PAINTINGS[n](d, BY_ID[n])
    return d


def decal(n, px=PX):
    """Painting n as its decal (RGBA, chips clear), or None if it is not drawn yet."""
    d = drawing(n)
    if d is None:
        return None
    return weather(raster(d, px), n * 31 + 7)


def lettered(n, px=PX):
    """Name n's lettering alone, as its decal (RGBA, chips clear), for a bomber whose painting is not made yet (the air
    thread's tallies.md, section 5: "A seat whose painting is not made yet can carry its name's lettering alone until
    the painting lands. The lettering is the cheap half"). The name in its own lettering (names.json) and in the
    hand its painter will have, its capitals 24 cm on her skin, in one line or two, worn as a painting is."""
    a = BY_ID.get(n)
    if a is None:
        return None

    def drawn(lines):
        d = Drawing(n * 7919 + 3, crude=crude(n))
        d.n = n
        names(d, a, (50.0, 30.0), 10.0, lines=lines)
        return d

    # (in one line, two or three, whichever lets its capitals stand tallest, up to 24 cm: the fewest lines that do)
    words = a["name"].split()
    ways = [split_lines(a["name"], k) for k in (1, 2, 3) if k <= len(words)]
    tall = [min(_fit_caps(drawn(lines)), 24.0) for lines in ways]
    best = next(i for i, t in enumerate(tall) if t >= max(tall) - 0.5)
    return weather(raster(drawn(ways[best]), px, caps_cm=24.0), n * 31 + 11)


def split_lines(name, k):
    """A name in k lines, broken where its longest line is shortest."""
    words = name.split()
    if k <= 1 or len(words) < 2:
        return [name]
    if k == 2:
        return split2(name)
    best = None
    for i in range(1, len(words) - 1):
        for j in range(i + 1, len(words)):
            lines = [" ".join(words[:i]), " ".join(words[i:j]), " ".join(words[j:])]
            key = max(len(t) for t in lines)
            if best is None or key < best[0]:
                best = (key, lines)
    return best[1] if best else split2(name)


def _fit_caps(d, px=PX, margin=0.035):
    """How tall its names' capitals would stand (cm on her skin) if drawing `d` were fitted into its decal."""
    xs, ys = [], []
    for op in d.ops:
        if op[0] == "polys":
            for poly in op[2]:
                xs += [p[0] for p in poly]
                ys += [p[1] for p in poly]
        else:
            x0, y0, x1, y1 = text_bounds(op[1])
            xs += [x0, x1]
            ys += [y0, y1]
    k = min(px[0] * (1 - 2 * margin) / (max(xs) - min(xs)), px[1] * (1 - 2 * margin) / (max(ys) - min(ys)))
    per_m = px[0] / SIZE_M[0]
    return min(100 * op[1]["size"] * k / per_m for op in d.ops if op[0] == "text")


def preview(n, path, px=900, bg=(76, 75, 53), alone=False):
    """Painting n on olive drab, to look at (or, `alone`, its name's lettering alone)."""
    im = (lettered if alone else decal)(n, (px, int(px * PX[1] / PX[0])))
    if im is None:
        return False
    base = Image.new("RGBA", im.size, bg + (255,))
    nz = smooth_noise(np.random.default_rng(n), (im.size[1], im.size[0]), (40, 40))
    arr = np.asarray(base).astype(np.float32)
    arr[..., :3] *= (1.0 + 0.04 * nz)[..., None]
    base = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")
    base.alpha_composite(im)
    base.convert("RGB").save(path)
    return True


def write(folder, ids=None, letter=True):
    """Every painting drawn (or those of `ids`) as art-NNN.png in `folder`; with `letter`, every other number of the list
    as its name's lettering alone (name-NNN.png), until its painting lands; and the plain lists of both (done.json)."""
    os.makedirs(folder, exist_ok=True)
    made = []
    for n in sorted(ids or PAINTINGS):
        im = decal(n)
        if im is None:
            continue
        im.save(os.path.join(folder, f"art-{n:03d}.png"), optimize=True)
        made.append(n)
    done = sorted(set(made) | {int(f[4:7]) for f in os.listdir(folder) if f.startswith("art-") and f.endswith(".png")})
    if letter:
        for n in sorted(BY_ID):
            if n not in done:
                lettered(n).save(os.path.join(folder, f"name-{n:03d}.png"), optimize=True)
    names_ = sorted(n for n in BY_ID if n not in done and os.path.exists(os.path.join(folder, f"name-{n:03d}.png")))
    with open(os.path.join(folder, "done.json"), "w") as fh:
        json.dump({"done": done, "count": len(done), "of": len(ART), "lettered": names_, "size_m": list(SIZE_M), "px": list(PX),
                   "file": "art-NNN.png", "name_file": "name-NNN.png",
                   "about": "Nose art decals: one picture for each number of the air thread's list (names.json) that is painted. "
                            "Each is RGBA with a clear background and clear chips; it covers size_m of her skin (the painting "
                            "fitted inside), laid on the livery layer's art.port part (UVs 0 to 1, u forward along her side, v up). "
                            "Draw only from these numbers.",
                   "about_lettered": "A number in `lettered` has no painting yet: name-NNN.png is its name's lettering alone, "
                                     "the same size and place, for a campaign bomber that carries that name (the air thread's "
                                     "tallies.md, section 5). When its painting lands it moves to `done`; use art-NNN.png then."},
                  fh, indent=1)
    with open(os.path.join(folder, "done.txt"), "w") as fh:
        fh.write("\n".join(str(n) for n in done) + "\n")
    return made


# ---- things -----------------------------------------------------------------------------------------------------------
def star(d, x, y, r, paint="art_white", points=5, turn=0.0, ink=None, inner=0.42):
    pts = []
    for k in range(points * 2):
        a = math.radians(90 + turn + 180.0 * k / points)
        rr = r if k % 2 == 0 else r * inner
        pts.append((x + rr * math.cos(a), y + rr * math.sin(a)))
    return d.shape(paint, pts, ink=ink)


def twinkle(d, x, y, r, paint="art_white"):
    """A small four-pointed star, as painters scattered them on a night sky."""
    d.shape(paint, [(x, y - r), (x + r * 0.22, y - r * 0.22), (x + r, y), (x + r * 0.22, y + r * 0.22), (x, y + r),
                    (x - r * 0.22, y + r * 0.22), (x - r, y), (x - r * 0.22, y - r * 0.22)], ink=0.0)


def bolt(d, x0, y0, x1, y1, wide=3.0, paint="art_yellow", ink=None, zig=3):
    """A lightning bolt from (x0, y0) to its point at (x1, y1), `wide` across its top: the painter's two kinks."""
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L                              # (along it)
    nx, ny = -uy, ux                                     # (across it)
    w = wide / 0.4
    shape_ = [(0.0, -0.2), (0.0, 0.2), (0.4, 0.06), (0.4, 0.22), (1.0, -0.06), (0.52, 0.04), (0.52, -0.14)]
    return d.shape(paint, [(x0 + ux * L * t + nx * w * c, y0 + uy * L * t + ny * w * c) for t, c in shape_], ink=ink)


def bomb(d, x, y, long, turn=0.0, paint="art_dark", band="art_yellow", fins=True, ink=None):
    """A bomb, nose at (x, y), `long` on the grid, pointing at `turn` degrees."""
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + (u * c - v * s) * long, y + (u * s + v * c) * long)

    if fins:
        d.shape(paint, [P(-0.74, 0.07), P(-0.98, 0.21), P(-1.04, 0.21), P(-1.0, 0.0), P(-1.04, -0.21), P(-0.98, -0.21), P(-0.74, -0.07)], ink=ink)
        d.shape("art_grey", [P(-0.84, 0.03), P(-1.0, 0.03), P(-1.0, -0.03), P(-0.84, -0.03)], ink=0.3)
    body = d.fill(paint, [P(0.0, 0.0), P(-0.06, 0.12), P(-0.24, 0.17), P(-0.66, 0.16), P(-0.8, 0.08), P(-0.8, -0.08), P(-0.66, -0.16), P(-0.24, -0.17), P(-0.06, -0.12)], ink=ink)
    d.line("art_lgrey", [P(-0.12, 0.09), P(-0.35, 0.125), P(-0.6, 0.115)], long * 0.03)        # (its shine, as the painter put it)
    if band:
        d.line(band, [P(-0.2, 0.165), P(-0.2, -0.165)], long * 0.045, taper=False)
        d.line(band, [P(-0.7, 0.13), P(-0.7, -0.13)], long * 0.035, taper=False)
    return P


def cloud(d, x, y, w, h, paint="art_white", seed=0, flat=True, ink=None, shade="art_lgrey"):
    """A fair-weather cloud of lumps, its middle at (x, y), w by h, its bottom flat if `flat`."""
    rng = random.Random(seed)
    lumps = []
    n = max(4, int(w / (h * 0.55)))
    for i in range(n):
        t = (i + 0.5) / n
        cx = x - w / 2 + w * t
        r = h * (0.32 + 0.22 * math.sin(math.pi * t)) * rng.uniform(0.85, 1.15)
        lumps.append((cx, y - h * 0.12 + r * 0.35 + h * 0.18 * math.sin(math.pi * t), r))
    if flat:
        lumps.append((x, y - h * 0.22, w * 0.42, h * 0.24))
    d.blobs(paint, lumps, ink=ink)
    if shade:
        d.line(shade, [(x - w * 0.38, y - h * 0.24), (x, y - h * 0.3), (x + w * 0.38, y - h * 0.24)], h * 0.12)
    return lumps


def puff(d, x, y, r, paint="art_lgrey", seed=0, ink=None, n=6):
    """A puff of smoke or steam: lumps round a middle."""
    rng = random.Random(seed)
    lumps = [(x, y, r * 0.75)]
    for i in range(n):
        a = 2 * math.pi * i / n + rng.uniform(-0.3, 0.3)
        lumps.append((x + r * 0.62 * math.cos(a), y + r * 0.52 * math.sin(a), r * rng.uniform(0.38, 0.55)))
    d.blobs(paint, lumps, ink=ink)


def speed(d, x, y, long, n=3, gap=2.4, paint="art_white", turn=0.0, w=0.9):
    """Speed lines trailing back from (x, y)."""
    for i in range(n):
        off = (i - (n - 1) / 2) * gap
        c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
        l = long * (1.0 - 0.25 * abs(off) / max(gap * n, 1))
        a = (x - off * s, y + off * c)
        b = (a[0] - l * c, a[1] - l * s)
        d.line(paint, [a, lerp(a, b, 0.5), b], w)


def wing(d, root, tip_dir, span, chord, paint="art_white", feathers=5, ink=None, spread=-26.0, shade="art_lgrey"):
    """A bird's wing from `root`, reaching `span` toward `tip_dir` (degrees): one silhouette, as a sign painter drew it.
    A smooth arched shoulder of coverts; under it `feathers` long round-ended primaries side by side, each from its own
    root along the arm, fanning from along the span at the tip to straight back at the root, toward the trailing side
    that `spread` names (negative: counter-clockwise of tip_dir). Shallow notches, so it reads as a wing, not a hand."""
    rx, ry = root
    a = math.radians(tip_dir)
    ux, uy = math.cos(a), math.sin(a)
    side = 1.0 if spread < 0 else -1.0
    nx, ny = -uy * side, ux * side                       # (toward the trailing edge)
    s, c = span, chord

    def P(X, Y):                                         # X along the span, Y toward the trailing edge, in grid units
        return (rx + ux * X + nx * Y, ry + uy * X + ny * Y)

    k = max(int(feathers), 3)
    base, tip, way = [], [], []
    for j in range(k):                                   # (j = 0 the outermost feather, at the tip)
        t = j / (k - 1)
        bx, by = s * (0.56 - 0.46 * t), c * (0.04 + 0.22 * t)
        ph = math.radians(7.0 + 66.0 * t)
        ln = s * (0.46 - 0.15 * t)
        base.append((bx, by))
        way.append((math.cos(ph), math.sin(ph)))
        tip.append((bx + ln * math.cos(ph), by + ln * math.sin(ph)))
    gaps = [math.hypot(tip[j + 1][0] - tip[j][0], tip[j + 1][1] - tip[j][1]) for j in range(k - 1)]
    r = 0.56 * sum(gaps) / len(gaps)                     # (each feather's end just overlapping the next)
    out = [(0.0, 0.0), (0.12 * s, -0.36 * c), (0.32 * s, -0.5 * c), (0.55 * s, -0.46 * c), (0.78 * s, -0.26 * c)]
    for j in range(k):
        dx, dy = way[j]
        if j < k - 1:
            px, py = tip[j + 1][0] - tip[j][0], tip[j + 1][1] - tip[j][1]
        else:
            px, py = tip[j][0] - tip[j - 1][0], tip[j][1] - tip[j - 1][1]
        pl = math.hypot(px, py) or 1.0
        px, py = px / pl, py / pl                        # (across, from this feather toward the next one rootward)
        for be in ((-95, -50, -10, 30, 70) if j == 0 else (-50, -10, 30, 70)):
            bb = math.radians(be)
            out.append((tip[j][0] + r * (dx * math.cos(bb) + px * math.sin(bb)), tip[j][1] + r * (dy * math.cos(bb) + py * math.sin(bb))))
        if j < k - 1:                                    # (a shallow notch between two feathers' ends)
            mx, my = lerp(tip[j], tip[j + 1], 0.5)
            wx, wy = lerp(way[j], way[j + 1], 0.5)
            out.append((mx - wx * r * 0.55, my - wy * r * 0.55))
    out += [(base[-1][0] - 0.04 * s, base[-1][1] + 0.3 * c), (0.0, 0.34 * c)]
    w0 = d.ink if ink is None else ink
    d.fill(paint, [P(X, Y) for X, Y in out], ink=ink, steps=3)
    for j in range(k - 1):                               # (each feather's edge: from its notch back toward the arm)
        mx, my = lerp(tip[j], tip[j + 1], 0.5)
        wx, wy = lerp(way[j], way[j + 1], 0.5)
        n0 = (mx - wx * r * 0.55, my - wy * r * 0.55)
        b0 = lerp(base[j], base[j + 1], 0.5)
        d.line("art_ink", [P(*n0), P(*lerp(n0, b0, 0.35)), P(*lerp(n0, b0, 0.7))], max(w0 * 0.9, c * 0.06))
    cov = [(0.0, 0.0), (0.12 * s, -0.36 * c), (0.32 * s, -0.5 * c), (0.55 * s, -0.46 * c), (0.7 * s, -0.34 * c)]
    n_sc = 4
    for j in range(n_sc):                                # (the coverts' scalloped lower edge, tip to root)
        t0, t1 = j / n_sc, (j + 1) / n_sc
        x0, x1 = 0.7 * s * (1 - t0), 0.7 * s * (1 - t1)
        y0, y1 = c * (-0.12 + 0.5 * t0), c * (-0.12 + 0.5 * t1)
        cov += [((x0 * 0.6 + x1 * 0.4), y0 * 0.5 + y1 * 0.5 + 0.2 * c), ((x0 * 0.2 + x1 * 0.8), y1 + 0.06 * c)]
    cov += [(0.0, 0.42 * c)]
    d.fill(shade, [P(X, Y) for X, Y in cov], ink=w0 * 0.85, steps=3)


# ---- a woman of the 1940s -------------------------------------------------------------------------------------------
HEADS = 7.4                      # (a figure stands seven and a half heads tall, as the painters drew them)
LEGS = {   # hip, knee, ankle of the leg on her facing side ("fore") and the other ("aft"), in heads, from her feet
    "stand": ([(0.40, 3.8), (0.34, 2.06), (0.30, 0.42)], [(-0.40, 3.8), (-0.16, 2.1), (-0.36, 0.46)]),
    "stride": ([(0.40, 3.8), (0.80, 2.12), (1.18, 0.44)], [(-0.40, 3.8), (-0.70, 2.08), (-1.1, 0.62)]),
    "together": ([(0.36, 3.8), (0.2, 2.06), (0.2, 0.42)], [(-0.36, 3.8), (-0.2, 2.06), (-0.2, 0.42)]),
    "sit": ([(0.35, 3.74), (1.8, 3.64), (1.94, 1.98)], [(0.08, 3.64), (1.54, 3.46), (1.5, 1.86)]),
    "astride": ([(0.12, 3.74), (1.1, 3.14), (0.9, 1.68)], None),
    "lean": ([(0.40, 3.8), (0.46, 2.06), (0.5, 0.42)], [(-0.40, 3.8), (-0.04, 2.08), (0.14, 0.5)]),
}
ARMS = {   # elbow, hand (wrist) from her shoulder, in heads (out from her side, up)
    "down": ((0.12, -1.25), (0.24, -2.36)),
    "hip": ((0.8, -0.95), (-0.08, -1.6)),
    "raise": ((0.22, 1.18), (0.34, 2.34)),
    "wave": ((0.95, -0.38), (1.22, 0.74)),
    "salute": ((0.8, 0.2), (-0.3, 1.2)),
    "hold": ((0.26, -1.18), (-0.5, -1.5)),
    "offer": ((0.42, -1.12), (1.22, -0.98)),
    "lift": ((0.95, 0.32), (1.5, 1.32)),
    "up_out": ((0.74, 0.95), (0.98, 2.12)),
    "lean": ((0.62, -0.92), (1.36, -0.44)),
    "rein": ((0.46, -1.1), (1.12, -1.32)),
    "chest": ((0.34, -1.12), (-0.38, -0.62)),
}
SHOULDER = (0.88, 5.92)


class Fig:
    """A figure's frame on the grid: her feet at (x, y), `h` tall, facing s (+1 to the right), `wide` her build."""

    def __init__(self, d, x, y, h, s=1, wide=1.0):
        self.d, self.x, self.y, self.k, self.s, self.wide = d, x, y, h / HEADS, s, wide
        self.hand = {}

    def P(self, u, v):
        return (self.x + self.s * u * self.k * self.wide, self.y + v * self.k)

    def Ps(self, pts):
        return [self.P(u, v) for u, v in pts]

    def r(self, v):
        return v * self.k


def hem_line(hw, y, n=6, amp=0.045):
    return [(-hw + 2 * hw * i / n, y + amp * math.cos(i * math.pi) + (0.03 if i in (0, n) else 0.0)) for i in range(n + 1)]


def lady(d, x, y, h, s=1, pose="stand", arms=("down", "down"), wear=None, hair="rolls", hair_paint="art_hair", hat=None,
         skin="art_skin", wide=1.0, smile=True, legs_shown=True):
    """A woman of the 1940s standing `h` tall with her feet at (x, y), facing s, fully dressed in `wear`:
       top, skirt or trousers (hem: knee, calf, long), sleeves (short, long, puff, rolled), collar, tie, belt, buttons,
       coat, jacket, bib (overalls), apron, sailor (a middy's collar), yoke, ruffle, dots, shoes / boots / tall (boots).
    `arms`: her fore and aft arm (see ARMS). Returns her frame, with .hand[fore|aft] = (point, direction)."""
    F = Fig(d, x, y, h, s, wide)
    w = {"top": "art_red", "sleeves": "short", "hem": "knee", "flare": 0.32, "shoes": "art_brown"}
    w.update(wear or {})
    if "skirt" not in w and not w.get("trousers"):
        w["skirt"] = w["top"]
    P, R = F.P, F.r
    ink, fine = d.ink, d.ink * 0.7
    legs = LEGS[pose]
    sitting = pose in ("sit", "astride")
    coat, jacket = w.get("coat"), w.get("jacket")
    body = coat or jacket or w["top"]
    # ---- her hair behind her head (the waves to her shoulders)
    if hair in ("rolls", "long", "bob", "pony"):
        low = {"rolls": 6.02, "long": 5.4, "bob": 6.2, "pony": 6.45}[hair]
        back = [(-0.5, 7.0), (-0.66, 6.6), (-0.7, 6.2)]
        n = 5
        for i in range(n + 1):
            t = i / n
            xx = -0.7 + 1.4 * t
            back.append((xx, low + (0.1 if i % 2 else -0.02)))
        back += [(0.7, 6.2), (0.66, 6.6), (0.5, 7.0), (0.0, 7.3)]
        d.fill(hair_paint, F.Ps(back), ink=fine, steps=4)
    if hair == "pony":
        d.limb(hair_paint, F.Ps([(-0.4, 7.25), (-0.82, 7.05), (-0.95, 6.5), (-0.86, 6.1)]), [R(0.17), R(0.18), R(0.14), R(0.06)], ink=fine)
    # ---- her legs
    shoe_kind = "boots" if w.get("boots") else "tall" if w.get("tall") else "shoes"
    shoe = w.get("boots") or w.get("tall") or w["shoes"]
    trousers = w.get("trousers")
    hemy = {"knee": 1.92, "calf": 1.26, "long": 0.14, "short": 2.36}[w["hem"]]

    def one_leg(pts, fore):
        hip, knee, ank = pts
        chain = [hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.3), ank]
        if trousers:
            rad = [0.46, 0.42, 0.31, 0.3, 0.28] if not w.get("jodhpurs") else [0.48, 0.56, 0.27, 0.2, 0.13]
            d.limb(trousers, F.Ps(chain), [R(r) for r in rad], ink=ink)
            if not w.get("jodhpurs"):
                d.line("art_ink", F.Ps([lerp(hip, knee, 0.35), knee, lerp(knee, ank, 0.75)]), R(0.035))
        else:
            d.limb(skin, F.Ps(chain), [R(r) for r in (0.4, 0.31, 0.185, 0.225, 0.11)], ink=fine)
        ax, ay = ank
        heel_up = pose == "stride" and not fore
        if shoe_kind in ("boots", "tall"):
            top = lerp(knee, ank, 0.45 if shoe_kind == "boots" else 0.06)
            d.limb(shoe, F.Ps([top, lerp(top, ank, 0.5), ank]), [R(0.24), R(0.2), R(0.16)], ink=ink)
            if shoe_kind == "boots":
                d.line(w.get("stitch", "art_cream"), F.Ps([lerp(top, ank, 0.2), lerp(top, ank, 0.45), lerp(top, ank, 0.75)]), R(0.04))
            foot = [(ax - 0.16, ay + 0.08), (ax + 0.14, ay + 0.04), (ax + 0.42, ay - 0.2), (ax + 0.5, ay - 0.3), (ax + 0.4, ay - 0.36),
                    (ax - 0.02, ay - 0.36), (ax - 0.18, ay - 0.3)] if not heel_up else \
                   [(ax - 0.14, ay + 0.1), (ax + 0.14, ay + 0.1), (ax + 0.26, ay - 0.36), (ax + 0.14, ay - 0.52), (ax - 0.06, ay - 0.44)]
            d.fill(shoe, F.Ps(foot), ink=fine, steps=3)
            d.shape("art_ink", F.Ps([(ax - 0.18, ay - 0.3), (ax - 0.04, ay - 0.32), (ax - 0.04, ay - 0.42), (ax - 0.16, ay - 0.42)]), ink=0.0)
            return
        if heel_up:
            foot = [(ax - 0.1, ay + 0.04), (ax + 0.1, ay + 0.06), (ax + 0.22, ay - 0.34), (ax + 0.14, ay - 0.5), (ax + 0.02, ay - 0.44), (ax - 0.12, ay - 0.1)]
        else:
            ln = 0.46 if fore else 0.36
            foot = [(ax - 0.11, ay - 0.02), (ax + 0.1, ay - 0.06), (ax + ln * 0.75, ay - 0.27), (ax + ln, ay - 0.37), (ax + ln * 0.78, ay - 0.42),
                    (ax + ln * 0.35, ay - 0.36), (ax + 0.02, ay - 0.27), (ax - 0.12, ay - 0.2)]
        d.fill(shoe, F.Ps(foot), ink=fine, steps=3)
        if not heel_up:
            d.shape(shoe, F.Ps([(ax - 0.12, ay - 0.22), (ax - 0.04, ay - 0.24), (ax - 0.05, ay - 0.42), (ax - 0.11, ay - 0.42)]), ink=fine * 0.8)

    if legs[1] and legs_shown:
        one_leg(legs[1], False)
    if legs_shown:
        one_leg(legs[0], True)
    # ---- her skirt (or her coat's), from her waist

    def skirt(paint, top_y, hem_y, flare_, ruffle=None, dots=None, folds=3):
        if sitting:
            kx, ky = legs[0][1]
            ax_, ay_ = legs[0][2]
            if hem_y < 0.8:                                  # (a gown falls from her knee to her feet)
                pts = [(-0.54, top_y), (-0.88, 4.0), (-0.84, 3.36), (kx - 0.3, ky - 0.36), (ax_ - 0.5, ay_ + 0.1), (ax_ - 0.72, ay_ - 0.42),
                       (ax_ + 0.95, ay_ - 0.46), (ax_ + 0.62, ay_ - 0.05), (kx + 0.36, ky - 0.05), (kx + 0.12, ky + 0.3), (0.62, 4.02), (0.54, top_y)]
                d.fill(paint, F.Ps(pts), ink=ink, steps=4)
                d.line("art_ink", F.Ps([(kx - 0.1, ky - 0.2), (ax_ - 0.15, (ky + ay_) / 2), (ax_ - 0.2, ay_ - 0.3)]), R(0.04))
                d.line("art_ink", F.Ps([(kx + 0.2, ky - 0.15), (ax_ + 0.25, (ky + ay_) / 2), (ax_ + 0.35, ay_ - 0.3)]), R(0.04))
            else:
                drop = 0.25 if hem_y > 1.5 else 0.6
                pts = [(-0.54, top_y), (-0.88, 4.0), (-0.84, 3.36), (kx - 0.1, ky - 0.32), (kx + 0.12, ky - 0.3 - drop),
                       (kx + 0.4, ky - 0.18 - drop), (kx + 0.34, ky + 0.12), (kx + 0.1, ky + 0.3), (0.62, 4.02), (0.54, top_y)]
                d.fill(paint, F.Ps(pts), ink=ink, steps=4)
                d.line("art_ink", F.Ps([(0.1, 4.2), (0.8, 3.72), (kx - 0.2, ky + 0.02)]), R(0.035))
            return
        hw = 0.86 + flare_ * (3.95 - hem_y) / 2.1
        hem = hem_line(hw, hem_y)
        pts = [(-0.52, top_y), (-0.86, 3.98)] + hem + [(0.86, 3.98), (0.52, top_y)]
        d.fill(paint, F.Ps(pts), ink=ink, steps=4)
        for i in range(folds):
            fx = (i - (folds - 1) / 2) * 0.56
            d.line("art_ink", F.Ps([(fx * 0.4, 3.7), (fx * 0.72, (3.7 + hem_y) / 2), (fx, hem_y + 0.16)]), R(0.035))
        if ruffle:
            band = [(xx * 1.03, yy - 0.02) for xx, yy in hem_line(hw * 1.03, hem_y, 10, 0.06)]
            top = [(xx, yy + 0.38) for xx, yy in reversed(hem_line(hw * 0.98, hem_y, 10, 0.03))]
            d.fill(ruffle, F.Ps(band + top), ink=fine, steps=3)
        if dots:
            rng = random.Random(7)
            for j in range(16):
                t = rng.random()
                yy = 3.8 - t * (3.8 - hem_y - 0.25)
                xx = (rng.random() * 2 - 1) * (0.62 + flare_ * t * 0.9)
                d.oval(dots, *P(xx, yy), R(0.07), R(0.07), ink=0.0)

    if coat:
        skirt(coat, 4.6, 1.68 if w["hem"] != "long" else 0.9, 0.16, folds=2)
        d.line("art_ink", F.Ps([(0.14, 4.6), (0.2, 3.0), (0.26, 1.74)]), R(0.04))
    elif w.get("skirt"):
        skirt(w["skirt"], 4.6, hemy, w["flare"], ruffle=w.get("ruffle"), dots=w.get("dots"))
    if w.get("apron") and not sitting:
        d.fill(w["apron"], F.Ps([(-0.56, 4.56), (-0.8, 3.0), (-0.74, hemy + 0.3), (0.74, hemy + 0.3), (0.8, 3.0), (0.56, 4.56)]), ink=fine, steps=3)
        d.line("art_ink", F.Ps([(-0.2, 4.3), (-0.26, 3.2), (-0.3, hemy + 0.5)]), R(0.03))
    if jacket:
        d.fill(jacket, F.Ps([(-0.54, 4.6), (-0.9, 3.95), (-0.98, 3.36), (-0.2, 3.26), (0.0, 3.44), (0.2, 3.26), (0.98, 3.36), (0.9, 3.95), (0.54, 4.6)]), ink=ink, steps=3)
    # ---- her body
    torso = [(-0.86, 3.86), (-0.8, 4.12), (-0.5, 4.6), (-0.66, 4.98), (-0.8, 5.25), (-0.83, 5.56), (-0.9, 5.92), (-0.5, 6.08), (-0.2, 6.12),
             (0.2, 6.12), (0.5, 6.08), (0.9, 5.92), (0.83, 5.56), (0.8, 5.25), (0.66, 4.98), (0.5, 4.6), (0.8, 4.12), (0.86, 3.86)]
    d.fill(body, F.Ps(torso[2:-2]), ink=ink, steps=4)
    if trousers:                                         # (her trousers to her waist, over her blouse)
        d.fill(trousers, F.Ps([(-0.53, 4.68), (-0.82, 4.12), (-0.88, 3.84), (-0.62, 3.38), (0.0, 3.32), (0.62, 3.38), (0.88, 3.84), (0.82, 4.12), (0.53, 4.68)]), ink=ink)
        d.line("art_ink", F.Ps([(0.0, 4.45), (0.0, 3.55)]), R(0.035))
        if not w.get("bib"):
            d.fill(w.get("belt", trousers), F.Ps([(-0.55, 4.72), (0.55, 4.72), (0.55, 4.54), (-0.55, 4.54)]), ink=fine, steps=2)
    if w.get("bib"):
        d.fill(w["bib"], F.Ps([(-0.46, 4.52), (-0.44, 5.32), (0.44, 5.32), (0.46, 4.52)]), ink=fine, steps=2)
        for sx in (-1, 1):
            d.limb(w["bib"], F.Ps([(sx * 0.38, 5.26), (sx * 0.5, 5.7), (sx * 0.58, 6.08)]), [R(0.075)] * 3, ink=fine)
            d.oval(w.get("buttons", "art_lsteel"), *P(sx * 0.37, 5.22), R(0.06), R(0.06), ink=fine * 0.6)
        d.fill(w["bib"], F.Ps([(-0.17, 4.88), (0.17, 4.88), (0.17, 5.08), (-0.17, 5.08)]), ink=fine * 0.7, steps=2)
    if w.get("apron") and not sitting:
        d.fill(w["apron"], F.Ps([(-0.4, 4.54), (-0.42, 5.3), (0.42, 5.3), (0.4, 4.54)]), ink=fine, steps=2)
        for sx in (-1, 1):
            d.limb(w["apron"], F.Ps([(sx * 0.38, 5.26), (sx * 0.3, 5.8), (sx * 0.22, 6.1)]), [R(0.05)] * 3, ink=fine * 0.8)
    if w.get("yoke"):
        d.fill(w["yoke"], F.Ps([(-0.9, 5.88), (-0.5, 5.46), (0.0, 5.64), (0.5, 5.46), (0.9, 5.88), (0.6, 6.06), (-0.6, 6.06)]), ink=fine, steps=3)
    # (her neckline: skin to her collar bone, the paint's edge only along its foot)
    neck = w.get("neck", "v" if (coat or jacket or w.get("sailor")) else "round")
    nl = {"v": [(-0.21, 6.13), (-0.1, 5.86), (0.0, 5.62), (0.1, 5.86), (0.21, 6.13)],
          "square": [(-0.34, 6.13), (-0.33, 5.8), (0.0, 5.78), (0.33, 5.8), (0.34, 6.13)],
          "round": [(-0.25, 6.13), (-0.17, 5.95), (0.0, 5.87), (0.17, 5.95), (0.25, 6.13)]}[neck]
    d.shape(skin, F.Ps(nl + [(0.25, 6.3), (-0.25, 6.3)]), ink=0.0)
    d.line("art_ink", F.Ps(nl), R(0.045), taper=False)
    d.shape(skin, F.Ps([(-0.18, 6.1), (0.18, 6.1), (0.18, 6.5), (-0.18, 6.5)]), ink=0.0)
    for sx in (-1, 1):
        d.line("art_ink", F.Ps([(sx * 0.18, 6.14), (sx * 0.175, 6.32), (sx * 0.19, 6.5)]), R(0.04), taper=False)
    if w.get("sailor"):
        for sx in (-1, 1):
            d.shape(w["sailor"], F.Ps([(sx * 0.21, 6.13), (sx * 0.9, 5.94), (sx * 0.82, 5.5), (sx * 0.06, 5.5)]), ink=fine)
            d.line("art_white", F.Ps([(sx * 0.84, 5.62), (sx * 0.16, 5.62)]), R(0.045), taper=False)
        d.shape(w.get("tie", "art_red"), F.Ps([(-0.14, 5.58), (0.14, 5.58), (0.2, 5.22), (0.0, 5.0), (-0.2, 5.22)]), ink=fine)
    if w.get("collar"):
        for sx in (-1, 1):
            d.shape(w["collar"], F.Ps([(sx * 0.03, 5.98), (sx * 0.24, 6.16), (sx * 0.4, 6.04), (sx * 0.3, 5.72)]), ink=fine)
    if w.get("tie") and not w.get("sailor"):
        d.shape(w["tie"], F.Ps([(-0.07, 5.98), (0.07, 5.98), (0.11, 5.22), (0.0, 5.1), (-0.11, 5.22)]), ink=fine)
    if coat or jacket:
        lap = w.get("lapel", body)
        for sx in (-1, 1):
            d.shape(lap, F.Ps([(sx * 0.21, 6.14), (sx * 0.52, 6.1), (sx * 0.44, 5.72), (sx * 0.31, 5.68), (sx * 0.03, 5.12)]), ink=fine)
    if w.get("belt"):
        d.fill(w["belt"], F.Ps([(-0.56, 4.7), (0.56, 4.7), (0.56, 4.48), (-0.56, 4.48)]), ink=fine, steps=2)
        d.shape(w.get("buckle", "art_gold"), F.Ps([(-0.1, 4.74), (0.1, 4.74), (0.1, 4.44), (-0.1, 4.44)]), ink=fine * 0.8)
    if w.get("buttons") and not w.get("bib"):
        for yy in ((5.42, 5.06, 4.24, 3.88) if (coat or jacket) else (5.62, 5.27, 4.92)):
            d.oval(w["buttons"], *P(0.15, yy), R(0.055), R(0.055), ink=fine * 0.6)
    # ---- her head
    d.oval(skin, *P(0.0, 6.86), R(0.44), R(0.54), ink=fine)
    fx = 0.07                                            # (her face turned a little to her facing side)
    for ex, big in ((-0.17 + fx, 0.86), (0.18 + fx, 1.0)):
        d.line("art_ink", F.Ps([(ex - 0.11 * big, 6.93), (ex, 6.985), (ex + 0.11 * big, 6.95)]), R(0.05), taper=False)
        d.oval("art_ink", *P(ex + 0.01, 6.89), R(0.05 * big), R(0.072), ink=0.0)
        d.line("art_ink", F.Ps([(ex + 0.1 * big, 6.95), (ex + 0.16 * big, 7.0)]), R(0.025), taper=False)
        d.line("art_ink", F.Ps([(ex - 0.11, 7.12), (ex + 0.01, 7.19), (ex + 0.12, 7.16)]), R(0.028))
    d.line("art_ink", F.Ps([(fx + 0.07, 6.82), (fx + 0.1, 6.73), (fx + 0.04, 6.7)]), R(0.03), taper=False)
    lip = [(fx - 0.13, 6.54), (fx - 0.05, 6.585), (fx, 6.565), (fx + 0.05, 6.585), (fx + 0.13, 6.54), (fx, 6.47)]
    d.fill("art_red", F.Ps(lip), ink=0.0, steps=3)
    d.line("art_ink", F.Ps([(fx - 0.12, 6.545), (fx, 6.53), (fx + 0.12, 6.545)] if smile else [(fx - 0.1, 6.53), (fx + 0.1, 6.53)]), R(0.022), taper=False)
    d.oval("art_pink", *P(fx + 0.25, 6.66), R(0.075), R(0.045), ink=0.0)
    d.oval("art_pink", *P(fx - 0.24, 6.66), R(0.06), R(0.045), ink=0.0)
    # ---- her hair over her head
    if hair in ("rolls", "long"):
        d.fill(hair_paint, F.Ps([(-0.47, 6.72), (-0.54, 7.2), (-0.42, 7.5), (-0.1, 7.66), (0.3, 7.6), (0.52, 7.36), (0.5, 6.85),
                                 (0.4, 7.1), (0.18, 7.22), (-0.08, 7.24), (-0.3, 7.12), (-0.42, 6.95)]), ink=fine)
        d.line("art_ink", F.Ps([(-0.3, 7.3), (-0.12, 7.48), (0.12, 7.47), (0.28, 7.34)]), R(0.035))
        d.line("art_ink", F.Ps([(-0.4, 7.12), (-0.38, 7.3), (-0.26, 7.42)]), R(0.03))
    elif hair in ("bob", "pony", "bun", "braids"):
        d.fill(hair_paint, F.Ps([(-0.47, 6.72), (-0.52, 7.2), (-0.32, 7.48), (0.05, 7.52), (0.38, 7.44), (0.52, 7.15), (0.48, 6.8),
                                 (0.34, 7.1), (0.0, 7.2), (-0.32, 7.08)]), ink=fine)
        d.line("art_ink", F.Ps([(0.1, 7.48), (-0.05, 7.33), (-0.3, 7.24)]), R(0.03))
        if hair == "bun":
            d.oval(hair_paint, *P(-0.06, 7.6), R(0.24), R(0.18), ink=fine)
        if hair == "braids":
            for sx in (-1, 1):
                for i in range(5):
                    d.oval(hair_paint, *P(sx * 0.48, 6.7 - 0.2 * i), R(0.1), R(0.12), ink=fine * 0.8)
                d.oval(w.get("ribbon", "art_red"), *P(sx * 0.48, 5.7), R(0.09), R(0.055), ink=fine * 0.6)
    # ---- her hat
    if hat == "garrison":
        cap = w.get("cap", body)
        d.fill(cap, F.Ps([(-0.52, 7.3), (0.2, 7.38), (0.52, 7.5), (0.42, 7.72), (-0.1, 7.76), (-0.48, 7.58)]), ink=fine, steps=3)
        d.line("art_ink", F.Ps([(-0.44, 7.44), (0.0, 7.5), (0.4, 7.58)]), R(0.03))
    elif hat == "straw":
        col = w.get("hat", "art_yellow")
        d.fill(col, F.Ps([(-1.02, 7.3), (-0.6, 7.42), (0.0, 7.46), (0.6, 7.42), (1.02, 7.3), (0.6, 7.18), (0.0, 7.14), (-0.6, 7.18)]), ink=fine, steps=4)
        d.fill(col, F.Ps([(-0.46, 7.38), (-0.42, 7.74), (0.0, 7.82), (0.42, 7.74), (0.46, 7.38)]), ink=fine, steps=4)
        d.fill(w.get("band", "art_red"), F.Ps([(-0.46, 7.38), (-0.45, 7.5), (0.45, 7.5), (0.46, 7.38)]), ink=fine * 0.7, steps=2)
    elif hat == "cowgirl":
        col = w.get("hat", "art_white")
        d.fill(col, F.Ps([(-0.98, 7.5), (-0.72, 7.38), (0.0, 7.34), (0.72, 7.38), (0.98, 7.5), (0.84, 7.3), (0.0, 7.18), (-0.84, 7.3)]), ink=fine, steps=4)
        d.fill(col, F.Ps([(-0.45, 7.38), (-0.5, 7.86), (-0.2, 7.94), (0.0, 7.84), (0.2, 7.94), (0.5, 7.86), (0.45, 7.38)]), ink=fine, steps=4)
        d.fill(w.get("band", "art_brown"), F.Ps([(-0.46, 7.4), (-0.47, 7.52), (0.47, 7.52), (0.46, 7.4)]), ink=fine * 0.7, steps=2)
    elif hat == "sailor":
        d.fill("art_white", F.Ps([(-0.4, 7.42), (-0.36, 7.66), (0.0, 7.72), (0.36, 7.66), (0.4, 7.42), (0.0, 7.36)]), ink=fine, steps=3)
        d.fill("art_white", F.Ps([(-0.52, 7.42), (-0.46, 7.3), (0.46, 7.3), (0.52, 7.42), (0.0, 7.48)]), ink=fine, steps=3)
    elif hat == "kerchief":
        col = w.get("hat", "art_blue")
        d.fill(col, F.Ps([(-0.5, 6.95), (-0.52, 7.36), (-0.24, 7.56), (0.1, 7.58), (0.44, 7.42), (0.52, 7.0), (0.38, 7.2), (0.0, 7.26), (-0.36, 7.18)]), ink=fine)
        d.oval(col, *P(0.0, 7.6), R(0.13), R(0.09), ink=fine)
        d.shape(col, F.Ps([(0.06, 7.6), (0.32, 7.78), (0.26, 7.55)]), ink=fine)
        d.shape(col, F.Ps([(-0.06, 7.6), (-0.3, 7.76), (-0.26, 7.53)]), ink=fine)
    elif hat == "beret":
        d.fill(w.get("hat", "art_navy"), F.Ps([(-0.52, 7.3), (-0.5, 7.56), (-0.1, 7.72), (0.46, 7.66), (0.56, 7.46), (0.4, 7.3)]), ink=fine)
    # ---- her arms, last: in front of all of her
    sleeves = w.get("sleeves", "short")
    sleeve = w.get("sleeve", body)
    for side, name in (("aft", arms[1]), ("fore", arms[0])):
        sx = 1.0 if side == "fore" else -1.0
        el, hd = ARMS[name]
        sh = (SHOULDER[0] * sx, SHOULDER[1])
        if sitting and name == "down":
            el, hd = (0.1, -1.2), ((0.3 if side == "fore" else 0.55), -2.1)
        E = (sh[0] + el[0] * sx, sh[1] + el[1])
        Hn = (sh[0] + hd[0] * sx, sh[1] + hd[1])
        chain = [sh, lerp(sh, E, 0.5), E, lerp(E, Hn, 0.45), Hn]
        if sleeves == "long":
            d.limb(sleeve, F.Ps(chain), [R(0.27), R(0.235), R(0.19), R(0.18), R(0.15)], ink=ink)
            cu = lerp(E, Hn, 0.84)
            dxy = (Hn[0] - E[0], Hn[1] - E[1])
            ln = math.hypot(*dxy) or 1.0
            nx, ny = -dxy[1] / ln * 0.17, dxy[0] / ln * 0.17
            d.line("art_ink", F.Ps([(cu[0] + nx, cu[1] + ny), (cu[0] - nx, cu[1] - ny)]), R(0.03), taper=False)
        else:
            d.limb(skin, F.Ps(chain), [R(0.215), R(0.18), R(0.135), R(0.14), R(0.095)], ink=fine)
            if sleeves == "rolled":
                d.limb(sleeve, F.Ps([sh, lerp(sh, E, 0.55), lerp(sh, E, 0.9)]), [R(0.27), R(0.235), R(0.2)], ink=ink)
                d.limb(sleeve, F.Ps([lerp(sh, E, 0.86), lerp(sh, E, 1.0)]), [R(0.21), R(0.2)], ink=fine)
            elif sleeves == "puff":
                d.oval(sleeve, *P(*lerp(sh, E, 0.2)), R(0.3), R(0.25), ink=ink)
            else:
                d.limb(sleeve, F.Ps([sh, lerp(sh, E, 0.45)]), [R(0.27), R(0.235)], ink=ink)
        # her hand, along her forearm
        dx, dy = Hn[0] - E[0], Hn[1] - E[1]
        ln = math.hypot(dx, dy) or 1.0
        ux, uy = dx / ln, dy / ln
        hc = (Hn[0] + ux * 0.12, Hn[1] + uy * 0.12)
        flat = name == "salute"
        ang = math.degrees(math.atan2(uy, ux * F.s * F.wide))
        d.oval(skin, *P(*hc), R(0.21 if flat else 0.155), R(0.095 if flat else 0.13), turn=ang, ink=fine)
        if not flat:
            d.oval(skin, *P(hc[0] - uy * 0.1 * sx, hc[1] + ux * 0.1 * sx), R(0.075), R(0.05), turn=ang + 50 * F.s, ink=fine * 0.8)
        F.hand[side] = (P(*hc), (ux * F.s, uy))
    return F


# ---- the paintings ----------------------------------------------------------------------------------------------------
PAINTINGS = {}


def painting(n):
    def put(fn):
        PAINTINGS[n] = fn
        return fn
    return put


def split2(name):
    """A name in two lines, broken where the longer line is shortest."""
    words = name.split()
    if len(words) < 2:
        return [name]
    best = min(range(1, len(words)), key=lambda i: max(len(" ".join(words[:i])), len(" ".join(words[i:]))))
    return [" ".join(words[:best]), " ".join(words[best:])]


def names(d, a, at, size, lines=None, gap=1.42, indent=0.0, turn=0.0, bend=0.0, sizes=None):
    """The list's name for painting `a`, in its own lettering, in one line or more (`lines`, which must join to
    the name), each line's middle on a column through `at`, `indent` moving each line along."""
    lines = lines or [a["name"]]
    assert " ".join(lines) == a["name"], (lines, a["name"])
    n = len(lines)
    for i, t in enumerate(lines):
        sz = sizes[i] if sizes else size
        dy = ((n - 1) / 2 - i) * gap * size
        d.name(t, a["lettering"], (at[0] + indent * (i - (n - 1) / 2), at[1] + dy), sz, turn=turn, bend=bend)


def night(d, x, y, rx, ry, paint="art_navy", seed=0, stars=5):
    """A patch of night sky behind a figure: a soft-cornered field, a few stars."""
    rng = random.Random(seed)
    pts = [(x + rx * math.cos(t) * (1 + 0.04 * rng.uniform(-1, 1)), y + ry * math.sin(t) * (1 + 0.04 * rng.uniform(-1, 1)))
           for t in (2 * math.pi * k / 14 for k in range(14))]
    d.fill(paint, pts, ink=d.ink * 1.2, steps=4)
    for k in range(stars):
        a_ = rng.uniform(0, 2 * math.pi)
        r_ = rng.uniform(0.55, 0.85)
        twinkle(d, x + rx * r_ * math.cos(a_), y + ry * r_ * math.sin(a_), rng.uniform(1.3, 2.1))


def bee(d, x, y, size, s=1, body="art_yellow", helmet=None, goggles=True, gloves=None, pose="wave", seat=False, scarf=None):
    """A cartoon honeybee, its middle at (x, y), `size` its body's length, facing s. `pose`: wave, guard (gloves up)."""
    k = size / 10.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    def oval_uv(cx, cy, rx, ry, turn, n=36):
        c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
        return [(cx + rx * math.cos(t) * c - ry * math.sin(t) * s_, cy + rx * math.cos(t) * s_ + ry * math.sin(t) * c) for t in (2 * math.pi * i / n for i in range(n))]

    def band_uv(cx, cy, rx, ry, turn, x0, x1, n=10):
        c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
        pts = [(x0 + (x1 - x0) * i / n, ry * math.sqrt(max(0.0, 1 - ((x0 + (x1 - x0) * i / n) / rx) ** 2))) for i in range(n + 1)]
        pts += [(x1 - (x1 - x0) * i / n, -ry * math.sqrt(max(0.0, 1 - ((x1 - (x1 - x0) * i / n) / rx) ** 2))) for i in range(n + 1)]
        return [(cx + u * c - v * s_, cy + u * s_ + v * c) for u, v in pts]

    # wings behind, buzzing
    d.fill("art_sky", Q(oval_uv(-1.0, 4.0, 3.3, 1.8, 62)), ink=d.ink * 0.8)
    d.fill("art_sky", Q(oval_uv(0.9, 4.3, 2.8, 1.5, 88)), ink=d.ink * 0.8)
    d.line("art_white", Q([(-1.4, 3.2), (-0.6, 5.0), (0.2, 6.4)]), 0.25 * k)
    # legs
    if seat:
        d.limb("art_ink", Q([(-0.4, -2.0), (0.6, -4.2), (0.0, -5.8)]), [0.36 * k, 0.32 * k, 0.28 * k], ink=0.0)
        d.oval("art_ink", *P(0.3, -6.0), 0.75 * k, 0.4 * k, ink=0.0)
    else:
        for u in (-1.2, 0.4):
            d.limb("art_ink", Q([(u, -2.4), (u + 0.3, -4.0), (u - 0.2, -5.3)]), [0.34 * k, 0.3 * k, 0.26 * k], ink=0.0)
            d.oval("art_ink", *P(u - 0.1, -5.5), 0.7 * k, 0.38 * k, ink=0.0)
    # the abdomen hanging back and down, its bands, its sting
    ab = (-2.0, -1.4, 3.7, 2.8, 37.0)
    d.shape("art_ink", Q([(-4.6, -3.2), (-6.6, -5.0), (-5.0, -2.6)]), ink=0.0)
    d.fill(body, Q(oval_uv(*ab)), steps=1)
    for u in (-2.2, -0.75, 0.7):
        d.shape("art_ink", Q(band_uv(*ab, u - 0.42, u + 0.42)), ink=0.0)
    head = P(1.9, 2.1)
    d.oval(body, *head, 2.4 * k, 2.3 * k)
    # its face
    d.oval("art_ink", *P(2.6, 2.0), 0.32 * k, 0.45 * k, ink=0.0)
    d.oval("art_ink", *P(3.6, 2.1), 0.28 * k, 0.42 * k, ink=0.0)
    d.line("art_ink", Q([(2.1, 0.9), (3.0, 0.55), (3.9, 1.0)]), 0.35 * k)
    d.oval("art_pink", *P(3.9, 1.35), 0.4 * k, 0.25 * k, ink=0.0)
    if helmet:
        d.fill(helmet, Q([(0.0, 2.6), (-0.5, 3.6), (0.6, 4.5), (2.0, 4.6), (3.4, 4.0), (4.1, 3.0), (3.0, 3.25), (1.8, 3.3), (0.8, 2.8)]), ink=d.ink * 0.8)
        d.limb(helmet, Q([(0.2, 2.8), (0.0, 1.6), (0.4, 0.6)]), [0.6 * k, 0.55 * k, 0.45 * k], ink=d.ink * 0.8)
        if goggles:
            d.line("art_ink", Q([(0.4, 3.35), (2.0, 3.65), (3.6, 3.4)]), 0.32 * k, taper=False)
            for u in (2.3, 3.5):
                d.oval("art_lsteel", *P(u, 3.6), 0.62 * k, 0.55 * k, ink=d.ink * 0.7)
                d.oval("art_sky", *P(u, 3.6), 0.4 * k, 0.36 * k, ink=0.0)
    else:
        for u in (1.2, 2.4):                                # (antennae)
            d.line("art_ink", Q([(u, 4.0), (u + 0.5, 5.4), (u + 1.3, 6.1)]), 0.3 * k)
            d.oval("art_ink", *P(u + 1.4, 6.2), 0.35 * k, 0.35 * k, ink=0.0)
    if scarf:
        d.fill(scarf, Q([(0.6, 0.6), (-1.2, 1.2), (-3.6, 2.2), (-5.4, 1.8), (-3.4, 1.0), (-1.2, 0.0), (0.9, 0.0)]), ink=d.ink * 0.8)
    # arms
    arms = {"wave": (((0.6, 0.0), (2.6, -1.0), (4.4, -0.3)), ((0.0, 0.4), (1.6, 2.4), (2.2, 4.6))),
            "guard": (((0.9, 0.0), (2.6, -1.4), (4.6, 0.4)), ((0.3, 0.5), (2.2, -0.2), (3.6, 2.6))),
            "rein": (((0.6, 0.0), (2.4, -1.2), (4.2, -1.2)), ((0.0, 0.4), (1.6, 2.4), (2.2, 4.6)))}[pose]
    hands = []
    for sh, el, hd in arms:
        d.limb("art_ink", Q([sh, el, hd]), [0.34 * k, 0.3 * k, 0.28 * k], ink=0.0)
        if gloves:
            d.oval(gloves, *P(hd[0] + 0.3, hd[1]), 1.05 * k, 0.95 * k, ink=d.ink * 0.9)
            d.oval(gloves, *P(hd[0] - 0.1, hd[1] + 0.55), 0.45 * k, 0.35 * k, ink=d.ink * 0.7)
            d.shape("art_white", Q([(hd[0] - 0.6, hd[1] - 0.5), (hd[0] - 0.3, hd[1] - 0.8), (hd[0] - 0.9, hd[1] - 1.1), (hd[0] - 1.1, hd[1] - 0.8)]), ink=d.ink * 0.6)
        else:
            d.oval("art_white", *P(*hd), 0.55 * k, 0.5 * k, ink=d.ink * 0.7)
        hands.append(P(*hd))
    return hands


def owl(d, x, y, size, s=1, body="art_brown", belly="art_tan", cap="art_lbrown"):
    """An owl standing, its feet at (x, y), `size` tall, facing s; in a flying cap with its goggles up."""
    k = size / 20.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    d.fill(body, [P(-4.6, 2.0), P(-5.6, 7.0), P(-5.2, 12.5), P(-4.4, 15.5), P(0.0, 17.0), P(4.4, 15.5), P(5.2, 12.5), P(5.6, 7.0), P(4.6, 2.0), P(0.0, 0.8)])
    d.fill(belly, [P(-3.2, 2.0), P(-3.8, 6.5), P(-2.8, 10.2), P(0.0, 11.0), P(2.8, 10.2), P(3.8, 6.5), P(3.2, 2.0), P(0.0, 1.4)], ink=d.ink * 0.8)
    for v, w_ in ((8.4, 2.0), (6.0, 2.4), (3.6, 2.0)):
        for u in (-1.4, 1.4):
            d.line(body, [P(u - w_ * 0.4, v + 0.5), P(u, v - 0.2), P(u + w_ * 0.4, v + 0.5)], 0.42 * k)
    # its face: a pale disc, big eyes, a beak
    d.fill("art_cream", [P(-4.4, 13.6), P(-4.2, 16.4), P(-2.2, 17.4), P(0.0, 16.4), P(2.2, 17.4), P(4.2, 16.4), P(4.4, 13.6), P(2.0, 11.6), P(0.0, 12.2), P(-2.0, 11.6)], ink=d.ink * 0.8)
    for u in (-2.1, 2.1):
        d.oval("art_yellow", *P(u, 14.4), 1.55 * k, 1.55 * k, ink=d.ink * 0.9)
        d.oval("art_ink", *P(u + 0.35, 14.2), 0.75 * k, 0.8 * k, ink=0.0)
        d.oval("art_white", *P(u + 0.6, 14.6), 0.22 * k, 0.22 * k, ink=0.0)
    d.shape("art_orange", [P(-0.6, 13.2), P(0.6, 13.2), P(0.0, 11.4)], ink=d.ink * 0.8)
    # its flying cap, buckled under, and its goggles pushed up on it
    d.fill(cap, [P(-4.9, 13.0), P(-5.0, 17.0), P(-3.0, 19.6), P(0.0, 20.3), P(3.0, 19.6), P(5.0, 17.0), P(4.9, 13.0), P(4.0, 15.6), P(2.2, 17.0), P(0.0, 17.4), P(-2.2, 17.0), P(-4.0, 15.6)])
    for u in (-1, 1):
        d.fill(cap, [P(u * 4.2, 15.0), P(u * 5.4, 13.2), P(u * 4.9, 11.0), P(u * 3.9, 12.2)], ink=d.ink * 0.8)
    d.line("art_ink", [P(-4.6, 17.6), P(0.0, 18.6), P(4.6, 17.6)], 0.45 * k, taper=False)
    for u in (-1.9, 1.9):
        d.oval("art_lsteel", *P(u, 18.5), 1.5 * k, 1.15 * k, ink=d.ink * 0.9)
        d.oval("art_sky", *P(u, 18.5), 1.05 * k, 0.78 * k, ink=0.0)
        d.line("art_white", [P(u - 0.5, 18.9), P(u + 0.1, 19.0)], 0.3 * k)
    # its feet
    for u in (-1.8, 1.8):
        for t in (-0.7, 0.0, 0.7):
            d.limb("art_orange", [P(u, 1.6), P(u + t, 0.0)], [0.42 * k, 0.32 * k], ink=d.ink * 0.7)
    return P


def mule(d, x, y, size, s=1, coat="art_lbrown", dark="art_brown", collar="art_red"):
    """A cartoon mule hauling, side on, its hooves at y, its chest at x, `size` long, facing s; ears flat back."""
    k = size / 30.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    for u0, lean in ((-16.5, -1.0), (-2.6, 2.0)):          # (the far legs, a little darker)
        d.limb(dark, [P(u0, 10.0), P(u0 + lean * 0.5, 5.0), P(u0 + lean, 1.2)], [1.4 * k, 1.0 * k, 0.8 * k], ink=d.ink * 0.8)
        d.fill("art_ink", [P(u0 + lean - 1.1, 1.6), P(u0 + lean + 1.1, 1.6), P(u0 + lean + 1.4, 0.0), P(u0 + lean - 1.3, 0.0)], ink=0.0)
    d.limb(dark, [P(-21.5, 14.5), P(-24.0, 12.0), P(-24.6, 8.0)], [0.7 * k, 0.6 * k, 0.45 * k], ink=d.ink * 0.8)
    d.oval("art_ink", *P(-24.7, 7.0), 1.1 * k, 1.6 * k, ink=0.0)
    d.fill(coat, [P(-21.5, 15.0), P(-21.0, 9.6), P(-15.0, 8.4), P(-5.0, 8.4), P(0.6, 9.4), P(1.6, 13.0), P(0.0, 16.4), P(-8.0, 17.2), P(-16.0, 17.6)])
    for u0, lean in ((-18.0, -1.6), (-1.6, 2.6)):           # (the near legs, braced)
        d.limb(coat, [P(u0, 11.0), P(u0 + lean * 0.5, 5.4), P(u0 + lean, 1.2)], [1.6 * k, 1.15 * k, 0.9 * k])
        d.fill("art_ink", [P(u0 + lean - 1.2, 1.7), P(u0 + lean + 1.2, 1.7), P(u0 + lean + 1.6, 0.0), P(u0 + lean - 1.4, 0.0)], ink=0.0)
    d.oval("art_cream", *P(-8.0, 9.6), 5.6 * k, 1.1 * k, ink=0.0)
    # neck, head down into the pull, ears flat back
    d.fill(coat, [P(-1.0, 15.6), P(2.6, 18.6), P(5.6, 19.0), P(7.4, 17.2), P(4.6, 13.4), P(1.4, 10.4)])
    d.line(dark, [P(-0.6, 16.6), P(2.4, 19.4), P(5.6, 19.8)], 1.1 * k)
    for du in (0.0, 1.2):
        d.fill(coat, [P(4.2 + du, 19.4), P(-0.2 + du, 21.6), P(-1.6 + du, 21.6), P(0.6 + du, 20.2), P(3.4 + du, 18.6)], ink=d.ink * 0.9)
    d.fill(coat, [P(4.0, 19.4), P(6.4, 20.0), P(10.4, 18.0), P(12.0, 15.6), P(11.2, 14.2), P(8.0, 14.6), P(5.0, 16.0)])
    d.oval("art_lgrey", *P(10.6, 15.6), 1.8 * k, 1.5 * k, ink=d.ink * 0.7)
    d.oval("art_ink", *P(11.2, 16.0), 0.35 * k, 0.25 * k, ink=0.0)
    d.line("art_ink", [P(9.4, 14.8), P(10.6, 14.4), P(11.8, 14.8)], 0.3 * k)
    d.oval("art_white", *P(6.8, 18.2), 0.9 * k, 0.75 * k, ink=d.ink * 0.6)
    d.line("art_ink", [P(6.0, 18.4), P(6.8, 18.0), P(7.6, 18.4)], 0.42 * k, taper=False)          # (a stubborn half-shut eye)
    d.oval("art_ink", *P(7.0, 17.75), 0.35 * k, 0.35 * k, ink=0.0)
    d.line("art_ink", [P(5.6, 19.4), P(7.0, 19.0), P(8.2, 19.2)], 0.45 * k)                         # (a scowl)
    # the collar and the traces back to the cart
    d.fill(collar, [P(1.0, 17.0), P(-1.2, 15.2), P(-0.4, 10.6), P(1.6, 10.2), P(3.0, 13.0), P(3.0, 16.4)], ink=d.ink * 0.9)
    d.line("art_ink", [P(0.4, 12.8), P(-10.0, 12.2), P(-23.0, 11.6)], 0.5 * k, taper=False)
    return P


def wheel(d, x, y, r, rim="art_red", spokes=8, hub="art_dark", ink=None):
    d.oval("art_dark", x, y, r, r, ink=ink)
    d.oval(rim, x, y, r * 0.8, r * 0.8, ink=d.ink * 0.6)
    d.oval("art_dark", x, y, r * 0.62, r * 0.62, ink=0.0)
    for i in range(spokes):
        a_ = math.pi * 2 * i / spokes
        d.line(rim, [(x, y), (x + r * 0.64 * math.cos(a_), y + r * 0.64 * math.sin(a_))], r * 0.12, taper=False)
    d.oval(hub, x, y, r * 0.18, r * 0.18, ink=d.ink * 0.6)


def lantern(d, x, y, size, glow=True):
    """A kerosene lantern hung from its bail at (x, y), `size` tall."""
    k = size / 10.0
    d.line("art_ink", [(x - 2.0 * k, y - 2.6 * k), (x, y), (x + 2.0 * k, y - 2.6 * k)], 0.5 * k, taper=False)
    d.fill("art_red", [(x - 1.6 * k, y - 2.4 * k), (x + 1.6 * k, y - 2.4 * k), (x + 1.0 * k, y - 3.4 * k), (x - 1.0 * k, y - 3.4 * k)], ink=d.ink * 0.8, steps=2)
    d.fill("art_yellow", [(x - 1.6 * k, y - 3.3 * k), (x - 2.0 * k, y - 5.4 * k), (x - 1.5 * k, y - 7.4 * k), (x + 1.5 * k, y - 7.4 * k), (x + 2.0 * k, y - 5.4 * k), (x + 1.6 * k, y - 3.3 * k)], ink=d.ink * 0.9, steps=3)
    d.oval("art_white", x, y - 5.4 * k, 0.6 * k, 1.0 * k, ink=0.0)
    for u in (-1.1, 0.0, 1.1):
        d.line("art_ink", [(x + u * k * 1.3, y - 3.4 * k), (x + u * k * 1.7, y - 5.4 * k), (x + u * k * 1.3, y - 7.3 * k)], 0.28 * k, taper=False)
    d.fill("art_red", [(x - 2.2 * k, y - 7.3 * k), (x + 2.2 * k, y - 7.3 * k), (x + 2.0 * k, y - 8.4 * k), (x - 2.0 * k, y - 8.4 * k)], ink=d.ink * 0.8, steps=2)
    if glow:
        for a_ in (200, 235, 270, 305, 340, 20, 160):
            c_, s_ = math.cos(math.radians(a_)), math.sin(math.radians(a_))
            cx, cy = x, y - 5.4 * k
            d.line("art_yellow", [(cx + 3.6 * k * c_, cy + 3.8 * k * s_), (cx + 5.6 * k * c_, cy + 5.8 * k * s_)], 0.55 * k)


def cowling(d, x, y, r):
    """A radial engine seen head on: its cowling's ring, nine cylinder heads inside it, its crankcase."""
    d.oval("art_olive", x, y, r, r)
    d.oval("art_dark", x, y, r * 0.84, r * 0.84, ink=d.ink * 0.8)
    for i in range(9):
        a_ = math.radians(90 + 40 * i)
        cx, cy = x + r * 0.58 * math.cos(a_), y + r * 0.58 * math.sin(a_)
        d.oval("art_steel", cx, cy, r * 0.17, r * 0.22, turn=math.degrees(a_) - 90, ink=d.ink * 0.6)
        d.line("art_dark", [(cx - r * 0.1 * math.sin(a_), cy + r * 0.1 * math.cos(a_)), (cx + r * 0.1 * math.sin(a_), cy - r * 0.1 * math.cos(a_))], r * 0.03, taper=False)
    d.oval("art_grey", x, y, r * 0.3, r * 0.3, ink=d.ink * 0.8)
    d.oval("art_dark", x, y, r * 0.12, r * 0.12, ink=d.ink * 0.5)
    d.line("art_lgrey", [(x - r * 0.75, y + r * 0.55), (x - r * 0.4, y + r * 0.86), (x + r * 0.05, y + r * 0.96)], r * 0.06)


def wrench(d, x0, y0, x1, y1, w, paint="art_lsteel"):
    """A monkey wrench from its handle's end (x0, y0) to its jaws (x1, y1)."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    ux, uy = dx / ln, dy / ln
    nx, ny = -uy, ux
    d.limb("art_red", [(x0, y0), (x0 + dx * 0.45, y0 + dy * 0.45)], [w * 0.75, w * 0.7])
    d.shape(paint, [(x0 + dx * 0.4 + nx * w * 0.42, y0 + dy * 0.4 + ny * w * 0.42), (x1 - ux * w * 0.6 + nx * w * 0.42, y1 - uy * w * 0.6 + ny * w * 0.42),
                    (x1 - ux * w * 0.6 - nx * w * 0.42, y1 - uy * w * 0.6 - ny * w * 0.42), (x0 + dx * 0.4 - nx * w * 0.42, y0 + dy * 0.4 - ny * w * 0.42)])
    jx, jy = x1, y1
    d.shape(paint, [(jx - ux * w * 1.1 + nx * w * 0.5, jy - uy * w * 1.1 + ny * w * 0.5), (jx + nx * w * 0.5, jy + ny * w * 0.5), (jx + nx * w * 1.6 + ux * w * 0.3, jy + ny * w * 1.6 + uy * w * 0.3),
                    (jx + nx * w * 1.7 - ux * w * 0.9, jy + ny * w * 1.7 - uy * w * 0.9), (jx - ux * w * 1.1 + nx * w * 1.5, jy - uy * w * 1.1 + ny * w * 1.5)])
    d.shape(paint, [(jx - ux * w * 1.0 - nx * w * 0.5, jy - uy * w * 1.0 - ny * w * 0.5), (jx - nx * w * 0.5, jy - ny * w * 0.5), (jx - nx * w * 1.5 + ux * w * 0.2, jy - ny * w * 1.5 + uy * w * 0.2),
                    (jx - nx * w * 1.6 - ux * w * 0.9, jy - ny * w * 1.6 - uy * w * 0.9)])
    d.oval("art_grey", x0 + dx * 0.62, y0 + dy * 0.62, w * 0.45, w * 0.35, turn=math.degrees(math.atan2(dy, dx)), ink=d.ink * 0.6)


def gremlin(d, x, y, size, s=1, skin="art_lsteel", suit="art_red"):
    """A gremlin of our own: a small grinning imp with great pointed ears, sitting with his seat at (x, y), `size` tall."""
    k = size / 20.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    for u, kick in ((-1.6, -1.0), (1.8, 1.0)):              # (legs dangling over the cowling's lip, big bare feet)
        d.limb(suit, [P(u, 1.0), P(u + 0.4 * kick, -2.8), P(u + 0.6 * kick, -5.2)], [1.3 * k, 1.1 * k, 0.9 * k])
        d.fill(skin, [P(u + 0.6 * kick - 1.2, -4.6), P(u + 0.6 * kick + 2.6, -5.2), P(u + 0.6 * kick + 3.0, -6.6), P(u + 0.6 * kick - 1.2, -6.8)], ink=d.ink * 0.8)
    d.fill(suit, [P(-3.6, 0.0), P(-3.8, 4.8), P(-2.6, 8.4), P(2.6, 8.4), P(3.8, 4.8), P(3.6, 0.0), P(0.0, -1.0)])
    d.oval("art_white", *P(0.0, 5.2), 0.5 * k, 0.5 * k, ink=d.ink * 0.5)
    d.oval("art_white", *P(0.0, 3.2), 0.5 * k, 0.5 * k, ink=d.ink * 0.5)
    # head and ears
    for u in (-1, 1):
        d.fill(skin, [P(u * 3.4, 14.2), P(u * 6.0, 18.4), P(u * 7.6, 22.6), P(u * 5.4, 20.6), P(u * 2.8, 17.4)], steps=3)
        d.fill("art_pink", [P(u * 3.9, 15.6), P(u * 6.2, 20.2), P(u * 3.8, 17.6)], ink=0.0, steps=3)
    d.fill(skin, [P(-4.2, 10.4), P(-4.8, 14.0), P(-3.6, 17.6), P(0.0, 18.8), P(3.6, 17.6), P(4.8, 14.0), P(4.2, 10.4), P(0.0, 8.6)])
    for u in (-1.6, 1.8):
        d.oval("art_yellow", *P(u, 14.6), 1.35 * k, 1.5 * k, ink=d.ink * 0.8)
        d.oval("art_ink", *P(u + 0.4, 14.4), 0.55 * k, 0.75 * k, ink=0.0)
    d.line("art_ink", [P(-3.0, 16.7), P(-1.4, 16.4), P(-0.4, 15.9)], 0.45 * k, taper=False)
    d.line("art_ink", [P(0.8, 15.9), P(1.9, 16.5), P(3.4, 16.8)], 0.45 * k, taper=False)
    d.oval(skin, *P(0.4, 12.6), 1.1 * k, 0.8 * k, ink=d.ink * 0.6)                                  # (his nose)
    d.fill("art_ink", [P(-3.2, 11.4), P(-1.6, 10.0), P(0.4, 9.6), P(2.4, 10.0), P(3.6, 11.6), P(0.4, 10.9)], ink=0.0, steps=4)
    d.fill("art_white", [P(-2.6, 11.1), P(-1.4, 10.3), P(0.4, 10.1), P(2.2, 10.3), P(3.0, 11.3), P(0.4, 10.75)], ink=0.0, steps=4)
    d.line("art_ink", [P(-1.8, 10.5), P(-1.7, 11.0)], 0.2 * k, taper=False)
    d.line("art_ink", [P(0.4, 10.1), P(0.4, 10.75)], 0.2 * k, taper=False)
    d.line("art_ink", [P(2.2, 10.4), P(2.1, 11.0)], 0.2 * k, taper=False)
    # his mechanic's cap, on the back of his head
    d.fill("art_navy", [P(-3.4, 16.6), P(-3.0, 18.6), P(0.2, 19.6), P(3.0, 18.8), P(3.2, 17.2), P(0.0, 17.8)], ink=d.ink * 0.8)
    d.fill("art_navy", [P(2.4, 17.4), P(5.8, 17.6), P(6.0, 17.0), P(3.0, 16.6)], ink=d.ink * 0.7)
    # arms: one hugging the wrench up over his shoulder, one on his knee
    d.limb(skin, [P(-3.4, 7.4), P(-5.4, 4.6), P(-3.8, 2.0)], [0.75 * k, 0.65 * k, 0.6 * k], ink=d.ink * 0.8)
    d.oval(skin, *P(-3.6, 1.6), 1.0 * k, 0.9 * k, ink=d.ink * 0.8)
    wrench(d, *P(-1.0, 3.4), *P(9.0, 17.4), 0.95 * k)
    d.limb(skin, [P(3.4, 7.6), P(5.6, 6.0), P(4.4, 8.4)], [0.75 * k, 0.65 * k, 0.6 * k], ink=d.ink * 0.8)
    d.oval(skin, *P(4.6, 8.8), 1.0 * k, 0.95 * k, ink=d.ink * 0.8)
    return P


def corn(d, x, y, long, turn=30.0):
    """An ear of corn in its husk, its tip at (x, y), `long`, pointing at `turn` degrees."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + (u * c - v * s_) * long, y + (u * s_ + v * c) * long)

    for side in (-1, 1):                                    # (the husk peeled back)
        d.fill("art_green", [P(-0.56, 0.0), P(-0.7, side * 0.16), P(-0.98, side * 0.26), P(-0.82, side * 0.1), P(-0.98, side * 0.02), P(-0.6, side * 0.02)], ink=None, steps=3)
    d.fill("art_yellow", [P(0.0, 0.0), P(-0.05, 0.08), P(-0.2, 0.15), P(-0.5, 0.16), P(-0.66, 0.13), P(-0.66, -0.13), P(-0.5, -0.16), P(-0.2, -0.15), P(-0.05, -0.08)])
    for v in (-0.09, -0.03, 0.03, 0.09):
        d.line("art_gold", [P(-0.06, v * 0.6), P(-0.3, v * 1.05), P(-0.6, v)], long * 0.022, taper=True)
    for u in (-0.16, -0.28, -0.4, -0.52):
        d.line("art_gold", [P(u, -0.14), P(u + 0.01, 0.0), P(u, 0.14)], long * 0.016, taper=True)
    for side in (-1, 1):
        d.fill("art_lgreen", [P(-0.42, side * 0.1), P(-0.56, side * 0.2), P(-0.85, side * 0.17), P(-0.7, side * 0.06)], ink=None, steps=3)
    d.line("art_tan", [P(0.0, 0.0), P(0.08, 0.04), P(0.12, -0.02)], long * 0.025)
    return P


def ring(d, x, y, r, paint="art_red", stripe="art_white", rope="art_tan"):
    """A life ring, its middle at (x, y), `r` its outer radius."""
    d.oval(paint, x, y, r, r * 0.42)
    for a0 in (20, 110, 200, 290):                          # (its four white bands)
        pts = ellipse(x, y, r, r * 0.42, 0, 8, a0, a0 + 32) + list(reversed(ellipse(x, y, r * 0.6, r * 0.25, 0, 8, a0, a0 + 32)))
        d.shape(stripe, pts, ink=0.0)
    d.oval("art_navy", x, y + r * 0.04, r * 0.6, r * 0.22, ink=d.ink * 0.8)
    for a0 in (55, 145, 235, 325):
        a_ = math.radians(a0)
        d.oval(rope, x + r * 0.98 * math.cos(a_), y + r * 0.42 * 0.98 * math.sin(a_), r * 0.08, r * 0.06, ink=d.ink * 0.5)


def waves(d, x0, x1, y, h, paint="art_blue", cap="art_white", n=4):
    pts = [(x0, y - h)]
    for i in range(n * 2 + 1):
        t = i / (n * 2)
        pts.append((x0 + (x1 - x0) * t, y + (h * 0.5 if i % 2 == 0 else -h * 0.2)))
    pts.append((x1, y - h))
    d.fill(paint, pts, steps=4)
    for i in range(n):
        t = (i * 2 + 0.6) / (n * 2)
        cx = x0 + (x1 - x0) * t
        d.line(cap, [(cx - (x1 - x0) / (n * 3.2), y + h * 0.18), (cx, y + h * 0.45), (cx + (x1 - x0) / (n * 5), y + h * 0.3)], h * 0.18)


def flowers(d, x, y, size, n=7, seed=3, paint="art_violet"):
    """An armful of bluebonnets: spikes of blue-violet buds with white tips, their stems gathered at (x, y)."""
    rng = random.Random(seed)
    for i in range(n):
        a_ = math.radians(60 + 70 * i / max(n - 1, 1) + rng.uniform(-6, 6))
        L = size * rng.uniform(0.75, 1.0)
        tx, ty = x + L * math.cos(a_), y + L * math.sin(a_)
        d.line("art_dgreen", [(x, y), (x + L * 0.5 * math.cos(a_), y + L * 0.5 * math.sin(a_)), (tx, ty)], size * 0.05, taper=False)
        spike = [(x + L * (0.55 + 0.45 * t) * math.cos(a_), y + L * (0.55 + 0.45 * t) * math.sin(a_)) for t in (0.0, 0.5, 1.0)]
        d.limb(paint, spike, [size * 0.11, size * 0.085, size * 0.03], ink=d.ink * 0.7)
        d.oval("art_white", *spike[2], size * 0.04, size * 0.05, ink=0.0)
        for t in (0.2, 0.45, 0.7):
            px, py = lerp(spike[0], spike[2], t)
            d.oval("art_lblue", px, py, size * 0.035, size * 0.03, ink=0.0)
    for j in range(3):
        a_ = math.radians(40 + 50 * j)
        d.fill("art_green", [(x, y), (x + size * 0.32 * math.cos(a_ - 0.3), y + size * 0.32 * math.sin(a_ - 0.3)), (x + size * 0.45 * math.cos(a_), y + size * 0.45 * math.sin(a_)),
                             (x + size * 0.3 * math.cos(a_ + 0.3), y + size * 0.3 * math.sin(a_ + 0.3))], ink=d.ink * 0.7, steps=3)


def derrick(d, x, y, h, paint="art_lbrown"):
    """A wooden oil derrick, its foot's middle at (x, y), `h` tall."""
    w0, w1 = h * 0.36, h * 0.07
    L, Rr = (x - w0 / 2, y), (x + w0 / 2, y)
    TL, TR = (x - w1 / 2, y + h), (x + w1 / 2, y + h)
    for a_, b_ in ((L, TL), (Rr, TR)):
        d.line(paint, [a_, b_], h * 0.035, taper=False)
    levels = [0.0, 0.22, 0.42, 0.6, 0.76, 0.9]
    for i in range(len(levels) - 1):
        t0, t1 = levels[i], levels[i + 1]
        p0l, p0r = lerp(L, TL, t0), lerp(Rr, TR, t0)
        p1l, p1r = lerp(L, TL, t1), lerp(Rr, TR, t1)
        d.line(paint, [p1l, p1r], h * 0.022, taper=False)
        d.line(paint, [p0l, p1r], h * 0.016, taper=False)
        d.line(paint, [p0r, p1l], h * 0.016, taper=False)
    for a_, b_ in ((L, TL), (Rr, TR)):
        d.line("art_ink", [a_, b_], h * 0.008, taper=False)
    d.fill(paint, [(TL[0] - h * 0.04, TL[1]), (TR[0] + h * 0.04, TR[1]), (TR[0] + h * 0.03, TR[1] + h * 0.05), (TL[0] - h * 0.03, TL[1] + h * 0.05)], ink=None, steps=2)
    # the gusher: black oil thrown up and falling back
    top = (x, y + h * 1.05)
    d.fill("art_black", [(x - h * 0.03, top[1]), (x - h * 0.06, top[1] + h * 0.18), (x - h * 0.2, top[1] + h * 0.3), (x - h * 0.28, top[1] + h * 0.22), (x - h * 0.34, top[1] + h * 0.1),
                         (x - h * 0.24, top[1] + h * 0.16), (x - h * 0.1, top[1] + h * 0.36), (x + h * 0.06, top[1] + h * 0.38), (x + h * 0.22, top[1] + h * 0.3),
                         (x + h * 0.32, top[1] + h * 0.12), (x + h * 0.24, top[1] + h * 0.18), (x + h * 0.08, top[1] + h * 0.2), (x + h * 0.03, top[1])], steps=4)
    for dx_, dy_ in ((-0.3, 0.02), (0.31, 0.0), (-0.22, -0.06)):
        d.oval("art_black", x + h * dx_, top[1] + h * dy_, h * 0.018, h * 0.03, ink=d.ink * 0.5)


def gauntlet(d, x, y, size, turn=0.0, paint="art_lsteel", shade="art_steel"):
    """A steel gauntlet's fist, its knuckles at (x, y), punching toward `turn` degrees, `size` long."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    k = size / 20.0

    def P(u, v):
        return (x + (u * c - v * s_) * k, y + (u * s_ + v * c) * k)

    d.fill(shade, [P(-20.0, -6.4), P(-20.6, 6.6), P(-12.0, 5.0), P(-11.6, -4.6)], steps=2)            # (the flared cuff)
    for u in (-17.0, -14.0):
        d.line("art_lsteel", [P(u, -5.4), P(u - 0.3, 5.6)], 0.6 * k, taper=False)
    d.fill(paint, [P(-12.6, -4.6), P(-12.6, 4.8), P(-7.0, 6.4), P(-1.4, 6.6), P(1.0, 5.2), P(1.4, -2.0), P(0.4, -5.6), P(-6.0, -6.0)], steps=3)
    for i, v in enumerate((4.6, 1.8, -1.0, -3.8)):          # (the curled fingers' plates)
        d.fill(paint if i % 2 == 0 else shade, [P(-3.0, v + 1.3), P(1.6, v + 1.2), P(2.4, v), P(1.6, v - 1.3), P(-3.0, v - 1.3)], ink=d.ink * 0.8, steps=2)
    d.fill(shade, [P(-9.0, 2.2), P(-4.0, 1.2), P(-0.6, -1.6), P(-1.2, -3.6), P(-5.6, -2.0), P(-9.6, -0.6)], ink=d.ink * 0.9, steps=3)    # (the thumb)
    for u in (-9.0, -6.0):
        d.line("art_ink", [P(u, 6.0), P(u + 0.2, 0.8)], 0.35 * k, taper=False)
        d.oval("art_dark", *P(u + 0.2, 4.4), 0.42 * k, 0.42 * k, ink=0.0)
    d.line("art_white", [P(-11.0, 5.6), P(-5.0, 6.2), P(0.2, 5.0)], 0.6 * k)


def locomotive(d, x, y, size, s=1, boiler="art_cream", trim="art_red"):
    """A steam locomotive running hard, side on, its pilot's tip at (x, y), `size` long, facing s."""
    k = size / 40.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    d.fill(boiler, [P(-24.0, 6.0), P(-24.0, 15.4), P(-6.0, 15.4), P(-3.4, 14.6), P(-2.2, 10.8), P(-3.4, 6.6), P(-6.0, 6.0)], steps=2)     # (the boiler)
    d.fill(boiler, [P(-3.6, 6.4), P(-2.0, 7.0), P(-1.4, 10.8), P(-2.0, 14.6), P(-3.6, 15.2), P(-4.4, 10.8)], ink=d.ink * 0.9)             # (its smokebox door)
    d.oval("art_yellow", *P(-1.6, 16.6), 1.6 * k, 1.3 * k, ink=d.ink * 0.9)                                                                # (the headlamp)
    d.oval("art_white", *P(-1.0, 16.8), 0.6 * k, 0.5 * k, ink=0.0)
    d.fill("art_black", [P(-8.6, 15.0), P(-9.2, 20.4), P(-10.6, 21.4), P(-5.0, 21.4), P(-6.2, 20.4), P(-6.6, 15.0)], ink=None, steps=2)      # (the stack)
    d.oval(boiler, *P(-14.0, 16.0), 2.6 * k, 1.8 * k, ink=None)                                                                             # (the dome)
    d.fill(trim, [P(-34.0, 6.0), P(-34.0, 22.0), P(-23.4, 22.0), P(-22.6, 19.6), P(-24.0, 19.6), P(-24.0, 6.0)], steps=2)                    # (the cab)
    d.fill("art_black", [P(-35.0, 21.6), P(-22.6, 22.6), P(-22.4, 23.6), P(-35.4, 22.8)], steps=2)
    d.fill("art_sky", [P(-31.6, 13.4), P(-31.6, 18.6), P(-26.4, 18.6), P(-26.4, 13.4)], ink=d.ink * 0.8, steps=2)
    d.line(trim, [P(-23.0, 13.2), P(-6.0, 13.2)], 0.8 * k, taper=False)
    d.line(trim, [P(-23.0, 8.0), P(-5.0, 8.0)], 0.8 * k, taper=False)
    d.fill(trim, [P(0.0, 0.0), P(-6.0, 6.2), P(-6.0, 0.0)], steps=1)                                                                       # (the pilot)
    for u in (-2.0, -4.0):
        d.line("art_ink", [P(u + 0.4, 0.4), P(u - 0.4, 4.2)], 0.4 * k, taper=False)
    d.fill("art_black", [P(-36.0, 0.0), P(-36.0, 6.4), P(-5.0, 6.4), P(-5.0, 2.0)], ink=d.ink * 0.8, steps=2)
    for u, r in ((-8.6, 3.0), (-17.0, 4.6), (-27.0, 4.6)):
        wheel(d, *P(u, max(r - 0.2, 2.6)), r * k, rim=trim, spokes=8)
    d.line("art_lsteel", [P(-17.0, 3.4), P(-27.0, 5.6)], 0.9 * k, taper=False)                                                              # (the driving rod)
    return P


def wagon(d, x, y, size, canvas="art_white", box="art_lbrown"):
    """A covered wagon, side on, facing right, its wheels on y, its middle at x, `size` long; the canvas patched."""
    k = size / 40.0

    def P(u, v):
        return (x + u * k, y + v * k)

    d.fill(box, [P(-19.0, 5.0), P(-18.0, 13.0), P(17.0, 13.0), P(18.6, 5.0)], steps=2)
    for v in (7.6, 10.2):
        d.line("art_ink", [P(-18.4, v), P(17.8, v)], 0.35 * k, taper=False)
    d.fill(canvas, [P(-17.0, 12.6), P(-19.6, 18.0), P(-17.6, 26.0), P(-10.0, 29.2), P(0.0, 30.0), P(10.0, 29.2), P(17.4, 26.0), P(19.2, 18.0), P(16.6, 12.6)], steps=4)
    for u in (-9.0, 0.0, 9.0):
        d.line("art_tan", [P(u - 1.0, 13.0), P(u * 1.1, 22.0), P(u * 1.05, 29.0)], 0.8 * k)
    for (u, v, w_, h_, col) in ((-12.6, 18.0, 4.8, 4.0, "art_red"), (4.0, 22.6, 5.2, 4.2, "art_blue"), (10.6, 15.4, 4.0, 3.6, "art_khaki")):
        d.shape(col, [P(u, v), P(u + w_, v + 0.3), P(u + w_ - 0.2, v + h_), P(u - 0.3, v + h_ - 0.2)], ink=d.ink * 0.7)
        for t in (0.2, 0.5, 0.8):
            d.line("art_ink", [P(u + w_ * t - 0.3, v - 0.4), P(u + w_ * t + 0.3, v + 0.4)], 0.22 * k, taper=False)
            d.line("art_ink", [P(u + w_ * t - 0.3, v + h_ - 0.5), P(u + w_ * t + 0.3, v + h_ + 0.3)], 0.22 * k, taper=False)
    wheel(d, *P(-12.0, 5.2), 6.0 * k, rim="art_red", spokes=10)
    wheel(d, *P(11.0, 4.4), 4.8 * k, rim="art_red", spokes=10)
    d.line("art_lbrown", [P(18.6, 6.0), P(25.0, 5.0), P(29.0, 4.4)], 0.9 * k, taper=False)                       # (the tongue)
    return P


def headset(d, x, y, size):
    """A radio headset: its band over the top, its two earphones, its cord; (x, y) the middle between the earphones."""
    k = size / 20.0
    band_out = ellipse(x, y + 1.0 * k, 10.6 * k, 12.0 * k, 0, 18, 8, 172)
    band_in = list(reversed(ellipse(x, y + 1.0 * k, 8.8 * k, 10.2 * k, 0, 18, 8, 172)))
    d.shape("art_brown", band_out + band_in)
    for u in (-1, 1):
        cx = x + u * 10.0 * k
        d.oval("art_dark", cx, y - 1.0 * k, 4.4 * k, 5.4 * k)
        d.oval("art_black", cx + u * 0.6 * k, y - 1.0 * k, 3.2 * k, 4.2 * k, ink=d.ink * 0.6)
        d.oval("art_lsteel", cx + u * 0.6 * k, y - 1.0 * k, 1.2 * k, 1.5 * k, ink=d.ink * 0.6)
        d.line("art_lgrey", [(cx - u * 2.6 * k, y + 2.8 * k), (cx - u * 1.4 * k, y + 3.8 * k)], 0.5 * k)
    d.line("art_black", [(x + 10.0 * k, y - 6.2 * k), (x + 9.0 * k, y - 10.0 * k), (x + 4.0 * k, y - 11.6 * k), (x - 2.0 * k, y - 10.4 * k)], 0.75 * k, taper=False)
    d.fill("art_lsteel", [(x - 2.0 * k, y - 9.6 * k), (x - 5.6 * k, y - 9.8 * k), (x - 5.6 * k, y - 11.2 * k), (x - 2.0 * k, y - 11.2 * k)], ink=d.ink * 0.8, steps=2)


def horseshoe(d, x, y, r, paint="art_lsteel", turn=0.0):
    """A horseshoe, open end down, its middle at (x, y), `r` its outer radius."""
    outer = ellipse(x, y, r, r * 1.08, turn, 20, -40, 220)
    inner = list(reversed(ellipse(x, y, r * 0.58, r * 0.66, turn, 20, -40, 220)))
    d.shape(paint, outer + inner)
    for a0 in (-10, 30, 70, 110, 150, 190):
        a_ = math.radians(a0 + turn)
        d.oval("art_ink", x + r * 0.79 * math.cos(a_), y + r * 0.87 * math.sin(a_), r * 0.06, r * 0.08, turn=a0 + turn, ink=0.0)
    d.line("art_white", [(x - r * 0.5, y + r * 0.86), (x, y + r * 1.0), (x + r * 0.5, y + r * 0.88)], r * 0.09)


def rooster(d, x, y, size, paint="art_black"):
    """A weathervane's rooster, cut from one sheet of iron, standing at (x, y), `size` tall, facing right."""
    k = size / 20.0

    def P(u, v):
        return (x + u * k, y + v * k)

    d.fill(paint, [P(-1.0, 0.0), P(-1.2, 3.0), P(-6.6, 6.4), P(-11.0, 13.6), P(-9.0, 17.2), P(-7.4, 12.6), P(-6.0, 15.4), P(-4.4, 10.8),
                   P(-1.8, 9.6), P(1.6, 12.0), P(1.8, 15.4), P(3.2, 17.4), P(3.4, 19.4), P(4.6, 18.4), P(5.4, 19.6), P(6.0, 18.0), P(5.6, 16.6),
                   P(7.6, 15.6), P(5.8, 14.8), P(6.0, 13.0), P(4.6, 12.4), P(4.4, 9.6), P(2.8, 5.6), P(1.4, 3.0), P(1.2, 0.0)], steps=3)
    d.oval("art_lsteel", *P(4.0, 16.2), 0.5 * k, 0.5 * k, ink=0.0)


def storm(d, x, y, w, h, paint="art_dark", seed=22):
    """A thunderhead, its middle at (x, y), w by h."""
    rng = random.Random(seed)
    lumps = []
    for i in range(7):
        t = i / 6
        lumps.append((x - w * 0.42 + w * 0.84 * t, y + h * 0.05 + h * 0.2 * math.sin(math.pi * t) * rng.uniform(0.8, 1.2), h * (0.28 + 0.12 * math.sin(math.pi * t))))
    lumps += [(x - w * 0.2, y + h * 0.36, h * 0.3), (x + w * 0.12, y + h * 0.4, h * 0.33), (x, y - h * 0.18, w * 0.4, h * 0.22)]
    d.blobs(paint, lumps)
    return lumps


def crescent(cx, cy, r, ix, iy, ir, n=120):
    """The part of the circle (cx, cy, r) outside the circle (ix, iy, ir)."""
    out_pts = [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    in_pts = [(ix + ir * math.cos(2 * math.pi * k / n), iy + ir * math.sin(2 * math.pi * k / n)) for k in range(n)]

    def arc(pts, keep):
        m = [keep(p) for p in pts]
        start = next(i for i in range(n) if m[i] and not m[i - 1])
        pts, m = pts[start:] + pts[:start], m[start:] + m[:start]
        return [p for p, k in zip(pts, m) if k]

    a_ = arc(out_pts, lambda p: math.hypot(p[0] - ix, p[1] - iy) > ir)
    b_ = arc(in_pts, lambda p: math.hypot(p[0] - cx, p[1] - cy) < r)
    one, two = a_ + list(reversed(b_)), a_ + b_
    return one if abs(area(one)) > abs(area(two)) else two


def small_bomber(d, x, y, long, paint="art_olive", turn=0.0):
    """A small four-engined bomber seen from above, flying toward `turn` degrees, its nose at (x, y)."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    k = long / 22.0

    def P(u, v):
        return (x + (u * c - v * s_) * k, y + (u * s_ + v * c) * k)

    d.fill(paint, [P(-17.0, 0.0), P(-16.6, 4.2), P(-18.2, 4.6), P(-18.4, -4.6), P(-16.6, -4.2)], ink=d.ink * 0.7, steps=2)      # (tailplane)
    d.fill(paint, [P(-9.0, 1.0), P(-10.0, 15.6), P(-11.8, 15.8), P(-12.4, 1.0), P(-12.4, -1.0), P(-11.8, -15.8), P(-10.0, -15.6), P(-9.0, -1.0)], ink=d.ink * 0.7, steps=2)
    d.fill(paint, [P(0.0, 0.0), P(-1.2, 1.2), P(-6.0, 1.6), P(-17.6, 0.8), P(-18.4, 0.0), P(-17.6, -0.8), P(-6.0, -1.6), P(-1.2, -1.2)], ink=d.ink * 0.7, steps=3)
    for v in (-10.4, -5.2, 5.2, 10.4):
        d.fill("art_dark", [P(-8.0, v + 0.8), P(-6.6, v + 0.6), P(-6.6, v - 0.6), P(-8.0, v - 0.8), P(-11.0, v - 0.7), P(-11.0, v + 0.7)], ink=d.ink * 0.5, steps=2)
    d.oval("art_sky", *P(-1.4, 0.0), 0.9 * k, 0.6 * k, turn=turn, ink=d.ink * 0.4)


# ---- batch 1 (numbers 1 to 24) ----------------------------------------------------------------------------------------
@painting(1)
def p1(d, a):    # a prairie whirlwind carrying a bomb in its funnel
    spine = [(16, 3), (13, 14), (16, 30), (21, 46), (23, 60), (22, 72)]
    radii = [1.4, 3.4, 7.0, 10.5, 14.0, 17.0]
    d.limb("art_lgrey", spine, radii)
    c = smooth(spine, False, 6)
    for i, frac in enumerate((0.16, 0.3, 0.44, 0.58, 0.72, 0.86)):
        j = int(frac * (len(c) - 1))
        cx, cy = c[j]
        rr = radii[min(int(frac * 5), 4)] * (1 - (frac * 5 % 1)) + radii[min(int(frac * 5) + 1, 5)] * (frac * 5 % 1)
        d.line("art_white" if i % 2 == 0 else "art_dark", ellipse(cx, cy, rr * 0.9, rr * 0.22, 4, 12, 195, 345), 0.9 + rr * 0.06)
    d.line("art_dark", [(12, 12), (15, 26), (19, 42), (19, 58)], 0.8)
    d.line("art_white", [(19, 18), (23, 34), (28, 50), (32, 66)], 0.9)
    bomb(d, 34, 34, 18, turn=-38)
    d.blobs("art_dark", [(4 + 7.2 * i, 76 + 3 * math.sin(i * 1.7), 6.5 + 1.6 * math.sin(i * 2.3)) for i in range(7)] + [(22, 71, 20, 5)])
    d.blobs("art_tan", [(8, 3, 4.2), (14, 2, 3.0), (20, 3.4, 4.0), (25, 2.4, 2.6), (3, 2, 2.4)])
    names(d, a, (88, 41), 14, lines=split2(a["name"]), indent=12, turn=6)


@painting(2)
def p2(d, a):    # a woman in a belted trench coat holding a lantern up into the dark
    night(d, 24, 43, 24, 41, seed=2, stars=6)
    F = lady(d, 22, 2, 68, 1, pose="stand", arms=("raise", "hip"),
             wear={"top": "art_tan", "coat": "art_tan", "belt": "art_brown", "buckle": "art_brown", "buttons": "art_brown", "sleeves": "long", "shoes": "art_brown", "lapel": "art_khaki"},
             hair="rolls", hair_paint="art_auburn")
    (hx, hy), _ = F.hand["fore"]
    lantern(d, hx + 0.6, hy - 0.6, 12)
    names(d, a, (84, 39), 14, lines=split2(a["name"]), indent=10, turn=4)


@painting(3)
def p3(d, a):    # a honeybee in flying helmet and goggles riding a bomb
    bomb(d, 104, 30, 70, turn=2)
    bee(d, 56, 47, 31, 1, helmet="art_brown", scarf="art_white", seat=True, pose="rein")
    lines = split2(a["name"])
    d.name(lines[0], a["lettering"], (62, 77), 14.5, bend=2.5)
    d.name(lines[1], a["lettering"], (64, 4), 15)


@painting(4)
def p4(d, a):    # a round-cheeked cloud blowing a small bomber along
    cloud(d, 28, 52, 54, 40, seed=4, flat=False)
    for u in (-1, 1):
        d.oval("art_pink", 24 + u * 8.5, 47, 4.4, 3.2, ink=0.0)
    for u in (-1, 1):
        d.oval("art_white", 24 + u * 6.2, 56, 2.6, 3.0, ink=0.5)
        d.oval("art_ink", 24 + u * 6.2 + 0.6, 55.6, 1.2, 1.6, ink=0.0)
        d.line("art_ink", [(24 + u * 6.2 - 2.4, 60.4), (24 + u * 6.2, 61.4), (24 + u * 6.2 + 2.4, 60.6)], 0.6)
    d.oval("art_red", 36.5, 49, 3.0, 2.4, ink=0.6)
    d.oval("art_ink", 37.2, 49, 1.1, 1.0, ink=0.0)
    for dy, ln_ in ((5.0, 34), (0.0, 44), (-5.0, 30)):
        d.line("art_white", [(42, 49 + dy * 0.4), (42 + ln_ * 0.5, 49 + dy + 1.2), (42 + ln_, 49 + dy * 1.4)], 1.5)
    small_bomber(d, 128, 54, 30, turn=6)
    names(d, a, (68, 9), 13.8)


@painting(5)
def p5(d, a):    # a woman in a service uniform and cap striding forward, saluting
    star(d, 30, 46, 34, "art_white", ink=0.7)
    lady(d, 28, 2, 72, 1, pose="stride", arms=("salute", "down"),
         wear={"top": "art_khaki", "jacket": "art_olive", "skirt": "art_olive", "collar": "art_khaki", "tie": "art_brown", "belt": "art_olive", "buckle": "art_gold",
               "buttons": "art_gold", "sleeves": "long", "cap": "art_olive", "shoes": "art_brown", "flare": 0.18},
         hair="rolls", hair_paint="art_hair", hat="garrison")
    names(d, a, (92, 40), 16, lines=split2(a["name"]), indent=10, turn=6)


@painting(6)
def p6(d, a):    # a woman in a long evening gown seated on a crescent moon
    d.shape("art_yellow", crescent(26, 44, 31, 40, 52, 26))
    d.line("art_gold", ellipse(26, 44, 27.5, 27.5, 0, 24, 150, 255), 1.0)
    for (x_, y_, r_) in ((60, 56, 2.0), (52, 84, 1.6), (6, 82, 1.8), (64, 40, 1.4)):
        twinkle(d, x_, y_, r_)
    lady(d, 26, -2, 54, 1, pose="sit", arms=("down", "wave"), wear={"top": "art_blue", "hem": "long", "sleeves": "puff", "shoes": "art_blue"},
         hair="long", hair_paint="art_blond")
    lines = split2(a["name"])
    d.name(lines[0], a["lettering"], (94, 70), 14, turn=4)
    d.name(lines[1], a["lettering"], (100, 18), 17, turn=4)


@painting(7)
def p7(d, a):    # a covered wagon with wings, its canvas patched
    wing(d, (44, 50), 128, 34, 12, "art_lgrey", shade="art_grey", spread=-30)
    wing(d, (40, 44), 146, 40, 13, "art_white", spread=-30)
    wagon(d, 70, 16, 60)
    speed(d, 26, 20, 14, n=3, gap=4.0, paint="art_white", w=1.0)
    lines = split2(a["name"])
    d.name(lines[0], a["lettering"], (80, 79), 16.4, turn=3)
    d.name(lines[1], a["lettering"], (70, -2), 16.7)


@painting(8)
def p8(d, a):    # a stubborn cartoon mule hauling a cart of bombs
    d.fill("art_lbrown", [(8, 32), (8, 44), (42, 44), (42, 32)], steps=2)
    for i, bx in enumerate((14, 25, 36)):
        bomb(d, bx, 64 + (i % 2) * 2, 26, turn=96 - 8 * (i - 1), band="art_yellow")
    d.fill("art_lbrown", [(6, 30), (6, 46), (44, 46), (44, 30)], steps=2)
    d.line("art_ink", [(6.5, 38), (43.5, 38)], 0.5, taper=False)
    wheel(d, 25, 30, 9, rim="art_red", spokes=8)
    mule(d, 94, 22, 56, 1)
    lines = split2(a["name"])
    names(d, a, (68, 0), 14, lines=lines)


@painting(9)
def p9(d, a):    # a farm girl in overalls and a straw hat leaning on a pitchfork
    F = lady(d, 22, 2, 72, 1, pose="lean", arms=("lean", "hip"),
             wear={"top": "art_red", "trousers": "art_lblue", "bib": "art_lblue", "sleeves": "rolled", "boots": "art_brown", "stitch": "art_tan", "hat": "art_yellow", "band": "art_blue"},
             hair="braids", hair_paint="art_blond", hat="straw")
    (hx, hy), _ = F.hand["fore"]
    fx = hx + 1.2
    d.line("art_lbrown", [(fx, 0.5), (fx, hy - 4), (fx, hy + 16)], 1.6, taper=False)
    d.line("art_ink", [(fx - 0.9, 0.5), (fx - 0.9, hy + 16)], 0.3, taper=False)
    d.fill("art_lsteel", [(fx - 4.2, hy + 16), (fx + 4.2, hy + 16), (fx + 4.2, hy + 18), (fx - 4.2, hy + 18)], ink=0.5, steps=2)
    for u in (-3.6, 0.0, 3.6):
        d.limb("art_lsteel", [(fx + u, hy + 17), (fx + u * 1.05, hy + 23), (fx + u * 1.1, hy + 27)], [0.75, 0.6, 0.25], ink=0.45)
    F2 = F.hand["fore"][0]
    d.oval("art_skin", F2[0], F2[1], 0.155 * F.k * 1.0, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (90, 40), 14, lines=split2(a["name"]), indent=10, turn=5)


@painting(10)
def p10(d, a):   # a steel gauntlet fist punching through a cloud
    for dy in (-9, 0, 9):
        d.line("art_white", [(4, 50 + dy * 1.2), (24, 50 + dy * 1.05), (40, 50 + dy)], 1.2)
    gauntlet(d, 110, 52, 62, turn=4)
    cloud(d, 44, 52, 44, 38, seed=10, flat=False)
    for a0 in (-50, -22, 8, 36, 64):
        r0, r1 = 13, 20
        cx, cy = 118, 52
        d.line("art_yellow", [(cx + r0 * math.cos(math.radians(a0)), cy + r0 * math.sin(math.radians(a0))), (cx + r1 * math.cos(math.radians(a0)), cy + r1 * math.sin(math.radians(a0)))], 1.4)
    names(d, a, (72, 9), 15)


@painting(11)
def p11(d, a):   # a grinning gremlin with a monkey wrench sitting on an engine cowling
    cowling(d, 24, 24, 21)
    gremlin(d, 23, 44, 36, 1, skin="art_tan", suit="art_red")
    names(d, a, (98, 42), 14, lines=split2(a["name"]), indent=6)


@painting(12)
def p12(d, a):   # an owl in a flying cap and goggles holding a navigator's ruler
    P = owl(d, 24, 2, 78, 1)
    k = 78 / 20.0
    rx0, ry0, rx1, ry1 = 6, 18, 50, 50
    ang = math.atan2(ry1 - ry0, rx1 - rx0)
    nx, ny = -math.sin(ang) * 2.4, math.cos(ang) * 2.4
    d.shape("art_cream", [(rx0 + nx, ry0 + ny), (rx1 + nx, ry1 + ny), (rx1 - nx, ry1 - ny), (rx0 - nx, ry0 - ny)])
    for t in (0.15, 0.3, 0.45, 0.6, 0.75, 0.9):
        px, py = rx0 + (rx1 - rx0) * t, ry0 + (ry1 - ry0) * t
        d.line("art_ink", [(px + nx * 0.9, py + ny * 0.9), (px + nx * (0.3 if t * 20 % 3 else 0.0), py + ny * (0.3 if t * 20 % 3 else 0.0))], 0.35, taper=False)
    d.fill("art_brown", [P(3.6, 13.0), P(6.6, 10.6), P(7.4, 6.6), P(5.6, 4.4), P(3.4, 6.0), P(4.0, 9.0)], ink=d.ink)
    names(d, a, (98, 36), 15, turn=7)


@painting(13)
def p13(d, a):   # a cowgirl in boots, skirt and hat astride a bomb, lasso overhead, one white star behind
    star(d, 40, 46, 33, "art_white", ink=0.7)
    bomb(d, 80, 22, 74, turn=6)
    F = lady(d, 40, 7, 60, 1, pose="astride", arms=("up_out", "rein"),
             wear={"top": "art_blue", "skirt": "art_brown", "boots": "art_red", "stitch": "art_white", "hat": "art_tan", "band": "art_red", "sleeves": "long", "collar": "art_white"},
             hair="rolls", hair_paint="art_auburn", hat="cowgirl")
    (hx, hy), _ = F.hand["fore"]
    d.line("art_tan", [(hx, hy), (hx + 2.5, hy + 4), (hx + 5, hy + 6.5)], 0.9)
    d.line("art_tan", ellipse(hx + 6, hy + 9.5, 10, 3.8, -8, 30), 1.1, taper=False)
    names(d, a, (110, 42), 14, lines=a["name"].split(), gap=1.36, indent=4)


@painting(14)
def p14(d, a):   # a woman in a western dress beside an oil derrick
    derrick(d, 14, 2, 52)
    lady(d, 34, 2, 72, 1, pose="stand", arms=("wave", "hip"),
         wear={"top": "art_red", "yoke": "art_white", "skirt": "art_red", "hem": "calf", "ruffle": "art_white", "flare": 0.5, "boots": "art_brown", "stitch": "art_tan",
               "hat": "art_white", "band": "art_red", "sleeves": "long"},
         hair="rolls", hair_paint="art_black", hat="cowgirl")
    names(d, a, (96, 40), 14, lines=split2(a["name"]), indent=10, turn=5)


@painting(15)
def p15(d, a):   # a rooster weather vane pointing at a falling bomb
    d.line("art_black", [(30, 8), (30, 54)], 2.0, taper=False)
    d.line("art_black", [(14, 30), (46, 30)], 1.5, taper=False)
    for x_ in (14, 46):
        d.oval("art_black", x_, 30, 1.8, 1.8)
    d.oval("art_black", 30, 30, 2.6, 2.6)
    d.line("art_black", [(0, 54), (66, 54)], 1.6, taper=False)
    d.shape("art_black", [(66, 57.6), (74, 54), (66, 50.4)])
    d.shape("art_black", [(0, 54), (-4, 59.6), (5, 59.6), (8, 54), (5, 48.4), (-4, 48.4)])
    rooster(d, 33, 55, 40)
    bomb(d, 104, 58, 28, turn=-90)
    for dx in (-5, 0, 5):
        d.line("art_white", [(104 + dx, 88), (104 + dx, 96)], 1.0)
    names(d, a, (100, 22), 14, lines=split2(a["name"]), indent=10)


@painting(16)
def p16(d, a):   # a woman in a sailor's middy dress waving from a life ring
    ring(d, 30, 24, 26)
    lady(d, 30, -15, 70, 1, pose="together", arms=("wave", "hold"),
         wear={"top": "art_white", "sailor": "art_navy", "tie": "art_red", "sleeves": "short", "shoes": "art_navy"},
         hair="bob", hair_paint="art_black", hat="sailor", skin="art_skin2", legs_shown=False)
    r = 26
    front = ellipse(30, 24, r, r * 0.42, 0, 18, 180, 360) + list(reversed(ellipse(30, 24, r * 0.6, r * 0.25, 0, 18, 180, 360)))
    d.shape("art_red", front)
    for a0 in (200, 290):
        pts = ellipse(30, 24, r, r * 0.42, 0, 8, a0, a0 + 32) + list(reversed(ellipse(30, 24, r * 0.6, r * 0.25, 0, 8, a0, a0 + 32)))
        d.shape("art_white", pts, ink=0.0)
    for i in range(4):                                       # (four breaking crests, each its own curl, white foam on top)
        x0 = -8 + 18.5 * i
        crest = [(x0, -2), (x0, 8.0), (x0 + 5, 12.6), (x0 + 10.5, 16.2), (x0 + 15.5, 16.8), (x0 + 18.5, 14.2),
                 (x0 + 15.6, 14.6), (x0 + 13.8, 12.0), (x0 + 16.5, 8.0), (x0 + 19, 6.0), (x0 + 21, -2)]
        d.shape("art_blue", smooth(crest, True, 3))
        d.line("art_white", [(x0 + 7.5, 14.0), (x0 + 12, 16.4), (x0 + 16, 16.5), (x0 + 18, 14.6)], 1.6)
    d.line("art_lblue", [(-4, 3), (20, 4.4), (42, 3.4), (66, 4.2)], 1.0)
    names(d, a, (102, 44), 15.2, lines=split2(a["name"]), indent=12, turn=5)


@painting(17)
def p17(d, a):   # a woman in a riding jacket holding up a big horseshoe
    F = lady(d, 26, 2, 70, 1, pose="stand", arms=("lift", "hip"),
             wear={"top": "art_white", "jacket": "art_red", "lapel": "art_red", "trousers": "art_tan", "jodhpurs": True, "tall": "art_black", "sleeves": "long",
                   "collar": "art_white", "buttons": "art_gold"},
             hair="rolls", hair_paint="art_hair")
    (hx, hy), _ = F.hand["fore"]
    horseshoe(d, hx + 2, hy + 10.5, 11, "art_lsteel")
    d.oval("art_skin", hx, hy, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    for a0 in (25, 55, 90, 125, 155):
        cx, cy = hx + 2, hy + 10.5
        d.line("art_yellow", [(cx + 14.5 * math.cos(math.radians(a0)), cy + 15.5 * math.sin(math.radians(a0))), (cx + 18.5 * math.cos(math.radians(a0)), cy + 19.5 * math.sin(math.radians(a0)))], 1.1)
    names(d, a, (98, 36), 14, lines=split2(a["name"]), indent=10, turn=5)


@painting(18)
def p18(d, a):   # a woman in a sun dress with an armful of bluebonnets
    F = lady(d, 24, 2, 72, 1, pose="together", arms=("chest", "hold"),
             wear={"top": "art_cream", "skirt": "art_cream", "sleeves": "puff", "sleeve": "art_white", "neck": "square", "belt": "art_blue", "buckle": "art_blue", "shoes": "art_white", "dots": "art_sky"},
             hair="long", hair_paint="art_hair")
    (hx, hy), _ = F.hand["fore"]
    (ax_, ay_), _ = F.hand["aft"]
    flowers(d, (hx + ax_) / 2, (hy + ay_) / 2 - 1, 26, n=7)
    for (px, py) in ((hx, hy), (ax_, ay_)):
        d.oval("art_skin", px, py, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (96, 40), 14, lines=split2(a["name"]), indent=10, turn=5)


@painting(19)
def p19(d, a):   # a steam locomotive bursting out of a cloud
    for i in range(4):
        puff(d, 40 - 7 * i, 82 - 2 * i, 4.5 + 1.0 * i, "art_lgrey", seed=19 + i)
    locomotive(d, 122, 36, 84, 1)
    cloud(d, 22, 54, 40, 40, seed=19, flat=False)
    speed(d, 128, 44, 10, n=3, gap=4.0, paint="art_white", turn=0.0, w=0.9)
    lines = split2(a["name"])
    names(d, a, (86, 12), 14, lines=lines, indent=10)


@painting(20)
def p20(d, a):   # a winged ear of corn in flight
    for dy in (-6, 0, 6):
        d.line("art_white", [(56 + dy * 0.6, 34 + dy), (68 + dy * 0.6, 41 + dy), (80 + dy * 0.6, 47 + dy)], 1.0)
    wing(d, (104, 62), 104, 28, 10, "art_lgrey", shade="art_grey", spread=-26)
    corn(d, 132, 80, 58, turn=32)
    wing(d, (98, 60), 120, 34, 11, "art_white", spread=-30)
    names(d, a, (46, 18), 15.2, lines=split2(a["name"]), indent=8)


@painting(21)
def p21(d, a):   # a bumblebee with boxing gloves and a buckeye nut on a string
    hands = bee(d, 26, 44, 40, 1, gloves="art_red", pose="guard")
    hx, hy = hands[0]
    d.line("art_tan", [(hx + 1.0, hy - 3.0), (hx + 1.6, hy - 10.0), (hx + 1.8, hy - 16.0)], 0.55, taper=False)
    d.oval("art_brown", hx + 1.8, hy - 21.0, 5.2, 5.0)
    d.oval("art_tan", hx + 0.8, hy - 19.4, 2.4, 2.0, ink=0.0)
    d.line("art_white", [(hx + 3.6, hy - 18.4), (hx + 4.8, hy - 20.4)], 0.7)
    names(d, a, (104, 42), 15, lines=split2(a["name"]), indent=8, turn=4)


@painting(22)
def p22(d, a):   # a storm cloud with a scowling face throwing lightning
    storm(d, 34, 58, 64, 40)
    for u in (-1, 1):
        d.oval("art_white", 34 + u * 8, 62, 3.4, 3.0, ink=0.5)
        d.oval("art_ink", 34 + u * 8 + 1.0, 61.4, 1.6, 1.8, ink=0.0)
        d.line("art_ink", [(34 + u * 13, 67.6), (34 + u * 8, 66.6), (34 + u * 3, 64.4)], 1.4, taper=False)
    d.line("art_ink", [(25, 50.6), (30, 53.6), (38, 53.8), (43, 50.4)], 1.3)
    bolt(d, 22, 42, 10, 6, 6.0, "art_yellow", zig=3)
    bolt(d, 46, 42, 58, 10, 6.0, "art_yellow", zig=3)
    names(d, a, (96, 20), 14, lines=split2(a["name"]), indent=12)


@painting(23)
def p23(d, a):   # a cook in an apron holding out a steaming dish shaped like a bomb
    F = lady(d, 22, 2, 78, 1, pose="stand", arms=("offer", "offer"),
             wear={"top": "art_blue", "skirt": "art_blue", "dots": "art_white", "apron": "art_white", "sleeves": "rolled", "shoes": "art_brown", "hat": "art_red"},
             hair="bun", hair_paint="art_hair", hat="kerchief", wide=1.18, skin="art_skin")
    (hx, hy), _ = F.hand["fore"]
    (ax_, ay_), _ = F.hand["aft"]
    px, py = (hx + ax_) / 2 + 7, max(hy, ay_) + 1.4
    d.oval("art_white", px, py, 17, 3.6)
    d.oval("art_lgrey", px, py + 0.4, 13.5, 2.3, ink=0.0)
    bomb(d, px + 13, py + 4.4, 27, turn=0, paint="art_brown", band="art_red")
    for dx in (-6, 0, 6):
        d.line("art_white", [(px + dx, py + 11), (px + dx - 1.6, py + 15), (px + dx + 1.2, py + 19), (px + dx - 0.6, py + 23)], 1.0)
    for (qx, qy) in ((hx, hy), (ax_, ay_)):
        d.oval("art_skin", qx, qy, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (90, 15), 13.6, lines=split2(a["name"]), indent=6)


@painting(24)
def p24(d, a):   # a radio headset with three lightning bolts
    headset(d, 34, 44, 40)
    bolt(d, 14, 64, 2, 84, 5.0, "art_yellow", zig=3)
    bolt(d, 34, 72, 36, 92, 5.0, "art_yellow", zig=3)
    bolt(d, 54, 64, 68, 84, 5.0, "art_yellow", zig=3)
    names(d, a, (100, 46), 14, lines=a["name"].split(), gap=1.36, indent=4)




def _more():
    """The later batches' paintings, each painter's in a file of its own under noseart_more/ (b2_*.py for numbers 25 to
    80, b3_* and b4_* after): each file does `from noseart import *` and puts its paintings with @painting(n), as the
    first 24 are put above. Loaded here, last, so that every helper above is there for them."""
    import importlib
    import pkgutil
    folder = os.path.join(HERE, "noseart_more")
    if not os.path.isdir(folder):
        return
    for info in sorted(pkgutil.iter_modules([folder]), key=lambda i: i.name):
        if not info.name.startswith("_"):                # (a painter's file still being worked on starts with _)
            try:
                importlib.import_module("noseart_more." + info.name)
            except Exception as e:                       # (one broken file never stops the rest being painted)
                import sys
                print(f"noseart: {info.name} not loaded: {type(e).__name__}: {e}", file=sys.stderr)


_more()
DONE = sorted(PAINTINGS)            # (the numbers whose painting exists: the scene draws only from these)
