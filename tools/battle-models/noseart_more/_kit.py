"""The shared figure kit for the nose art after the first 24: men, boys and the leather flying helmet.

    from noseart_more._kit import *          (in a painter's file: it brings everything of noseart's too)

IMPORT NOSEART FIRST. A painter's file under noseart_more/ is loaded by noseart itself, so it simply starts with the
line above. A script of one's own (a preview, a test) starts `import noseart` and only then imports this kit: were the
kit loaded first, noseart would load the painters' files while the kit is half-built, their `import *` would find
nothing, and their paintings would be left out without a word. So this kit refuses to load before noseart.

  man(d, x, y, h, ...)       a man of the 1940s, fully dressed, drawn in lady()'s own hand: her heads, her ink, her
                             face, her arms and legs, so that he and she can stand in one painting
  kid(d, x, y, h, ...)       a boy of about ten in the same hand: a bigger head for his height, a boy's clothes
  helmet(d, x, y, size, ...) a leather flying helmet with its goggles up on the brow: alone (big), or on a head (small);
                             its goggles' band runs round to the leather's edge and no further, its chin strap is leather
  scarf(d, F, paint, ...)    an airman's long scarf on any figure's frame, lady()'s too (call it after her): wound round
                             the neck, its tail 2 to 3 heads long streaming from the nape, rising in waves to a split,
                             fringed end; tail="ahead" streams it before the face (the wind at the back)

man() and kid() return the figure's frame, a Body (a Fig, as lady() returns): .hand["fore"] and .hand["aft"] are
(point, direction) as hers are; .grip(side) paints a hand again over a thing it holds (a bat, a rag, a cane); .mouth,
.face and .top are points on the grid; and three frames place things by him, all in heads (u toward the side he faces,
v up from his feet, as lady()'s Fig.P):
  P(u, v)    his body above the hips (it leans and sinks with his pose)
  PL(u, v)   his legs
  PH(u, v)   his head (his face, his hat): (0.07, 6.5) is his mouth, (0, 7.42) the top of his head

POSES. `pose` names his legs: every one of lady()'s (stand, stride, together, sit, astride, lean) and a man's own:
easy (her stand, the weight on one leg), wide (feet planted apart), bat (a batter's stance, knees bent), crouch (a
squat, knees apart), kneel (on the aft knee), curl (sitting curled up, knees to his chest), run, step (a high step).
`arms` names his fore and aft arm: every one of lady()'s (down, hip, raise, wave, salute, hold, offer, lift, up_out,
lean, rein, chest) and a man's own: swing (both hands on a bat swung level before him), bat (a bat cocked over his aft
shoulder), slide (a trombone: the fore hand out on the slide, the aft at his mouth), ring (a triangle held up and
struck), flip (a pan held out), point, reach, polish (a rag rubbed on a thing before him), bounce (a ball), throw, hug
(his knees, curled), tip (his hat's brim), mouth (a hand at his mouth), pocket, carry (a thing on his shoulder), tray,
rest (on a cane or a bar), fold. An arm can also be sent to a point: to(u, v) (heads, his body standing straight), or
to(u, v, frame="head"); u is toward the side he faces for either arm, so an aft hand sent to his own aft side takes a
negative u (pocket and tip do this: each hand to its own hip, its own side of his brim).

WEAR (colours are PAINTS names; leave a key out to leave the thing off):
  shirt (or top), sleeves (long, short, rolled; none is taken as short, for a shirt keeps its sleeves; bare: no sleeves,
  the suit cut deep round his arms on narrow straps: only for a period bathing suit, as motif 92's strongman wears),
  sleeve (their colour), neck (open, tie, crew, high), collar, tie, bowtie, kerchief (a neckerchief), scarf (an
  airman's long scarf: see scarf(); scarf_tail aft, the default, or ahead), trousers, shorts, knickers, socks, belt
  (None or False: no belt), buckle, braces, overalls (bib overalls), coveralls (one piece), apron (cut: apron_cut = bib
  or waist), vest, jacket (cut = suit, tunic, flight or short; lapel, fur: a sheepskin collar, knit: its waistband and
  cuffs, trim: a braid on a tunic's collar and cuffs), coat (to the knee), stripes (a striped jacket, coat or shirt,
  its sleeves striped with it when they are its colour: stripe_dir v, down him, the default with a jacket, or h, round
  him and round his sleeves, the default for a shirt), buttons, pocket (a handkerchief in the breast pocket), shoes,
  boots (tucked: True puts his trouser legs in them: straight shafts as wide as the leg, cut a little aslant at the top,
  the trousers bloused over it), gloves; hat colours: hat or cap, band; helmet (the flying helmet's leather), strap
  (False: the flying helmet's chin strap left off).
HATS: ball (a ball cap), paper (a short-order cook's folded paper hat), toque (a chef's tall hat), boater (straw),
  flying (a leather flying helmet, goggles up), goggles (goggles alone, on the brow), garrison, fedora, cap (a plain
  soft cap with a bill, a mechanic's), newsboy, straw (a farmer's), cowboy, peaked (a band's, a railroad's, a crew
  chief's), derby, souwester, shako (a drum major's).
HAIR: short (a 1940s cut: his forehead and temples showing, a side part notched into it over his aft brow, his
  forelock lifted over his fore brow, the sides short above his ears; pressed down under a hat's crown), slick (the
  same, combed flat and back, a shine on it), crew, tousle (a boy's), bald, none. FACE: smile, mouth (smile, flat, open,
  grin, o, none), eyes (open, shut, wink), moustache (trim, full), cheeks.
"""
import sys

if "noseart" not in sys.modules:     # (the trap in the note above: noseart first, then this kit)
    raise ImportError("import noseart before noseart_more._kit")

from noseart import *  # noqa: F401,F403,E402  (math, random, lerp, smooth, ellipse, Fig, HEADS, LEGS, ARMS, lady, ...)

import math  # noqa: E402  (noseart's own, named here too)

NECK = 6.1                           # (where his head meets his body: heads from his feet, in a grown man's frame)
UPPER, FORE = 1.22, 1.12             # (his upper arm and his forearm, heads, as hers are)
KID = 0.7                            # (a boy of ten: his body seven tenths of a man's, his head as big)
FACE_X = 0.07                        # (his face turned a little to the side he faces, as hers)


def to(u, v, bend="down", frame="body", hand=True, elbow=None):
    """An arm posed by where its hand goes: (u, v) in heads, u toward the side he faces and v up from his feet, his body
    standing straight (frame="head": in his head's frame, so that (0.07, 6.42) is just under his mouth); `bend` which
    way its elbow goes (down, up, out, in); hand=False leaves the hand unpainted (in a pocket). elbow=(out, up) fixes
    the elbow instead, in heads from his shoulder as an (elbow, hand) pair gives it (out from his side, mirrored for
    the aft arm): a hand brought up to his face keeps its elbow at his side (the mouth and slide presets).
    u points to the side he faces for BOTH arms: it is never mirrored for the aft arm (as an (elbow, hand) pair is). So
    an aft hand sent to his own aft side needs a negative u: to(0.74, 4.02) is his fore hip, to(-0.74, 4.02) his aft
    hip, and an aft arm given a positive u reaches across his body."""
    return {"to": (u, v), "bend": bend, "frame": frame, "hand": hand, "elbow": elbow}


# ---- poses (lady()'s, and a man's own) -----------------------------------------------------------------------------
# Legs: (fore, aft), each hip, knee and ankle in heads from his feet; arms: (elbow, hand) out from his side and up from
# his shoulder, as hers; or to() a point, or {"fore": ..., "aft": ...} for a two-handed pose.
MAN_LEGS = dict(LEGS)
MAN_LEGS.update({
    "easy": LEGS["stand"],                                                           # (her stand: his weight on one leg)
    "stand": ([(0.42, 3.8), (0.45, 2.08), (0.47, 0.42)], [(-0.42, 3.8), (-0.45, 2.1), (-0.47, 0.44)]),
    "wide": ([(0.44, 3.8), (0.76, 2.1), (1.0, 0.42)], [(-0.44, 3.8), (-0.76, 2.1), (-1.0, 0.44)]),
    "bat": ([(0.44, 3.48), (1.08, 2.0), (1.02, 0.42)], [(-0.44, 3.48), (-0.92, 1.98), (-1.06, 0.44)]),
    "crouch": ([(0.44, 2.16), (1.4, 2.4), (1.06, 0.42)], [(-0.44, 2.16), (-1.4, 2.4), (-1.06, 0.44)]),   # (a squat, knees apart)
    "kneel": ([(0.42, 2.62), (1.22, 2.06), (1.18, 0.42)], [(-0.42, 2.62), (-0.56, 0.62), (-1.36, 0.5)]),
    "curl": ([(0.3, 1.3), (1.1, 2.96), (1.2, 0.98)], [(0.04, 1.24), (0.86, 2.84), (0.94, 0.9)]),
    "run": ([(0.42, 3.8), (1.16, 2.62), (1.02, 1.08)], [(-0.42, 3.8), (-0.72, 2.16), (-1.56, 1.44)]),
    "step": ([(0.42, 3.8), (1.42, 3.36), (1.3, 1.74)], [(-0.42, 3.8), (-0.44, 2.1), (-0.48, 0.44)]),
})
POSE = {   # (how a pose carries him: his lean forward in degrees, which foot is up, whether he sits, legs before him)
    "stride": {"heel": "aft"}, "run": {"heel": "aft", "tilt": 9}, "step": {"heel": "fore"}, "bat": {"tilt": 5},
    "kneel": {"tilt": 3, "heel": "aft"}, "crouch": {"tilt": 2}, "curl": {"seated": True, "front": True, "tilt": 12},
    "sit": {"seated": True}, "astride": {"seated": True},
}
MAN_ARMS = dict(ARMS)
MAN_ARMS.update({
    "swing": {"fore": to(1.3, 4.66), "aft": to(1.08, 4.5)},
    "bat": {"fore": to(-0.7, 6.3, "down"), "aft": to(-0.86, 6.2, "out")},
    "slide": {"fore": to(3.0, 5.36), "aft": to(0.08, 6.32, frame="head", elbow=(0.06, -0.6))},   # (the aft hand at his lips)
    "ring": {"fore": to(1.46, 7.0, "out"), "aft": to(1.06, 6.02)},
    "flip": ((0.42, -1.02), (1.32, -0.74)),
    "point": ((0.98, 0.14), (2.02, 0.44)),
    "reach": ((0.98, -0.42), (2.08, -0.48)),
    "polish": {"fore": to(2.04, 5.24), "aft": to(1.6, 4.66)},
    "bounce": ((0.32, -1.08), (0.94, -1.92)),
    "throw": ((0.84, 0.24), (0.42, 1.32)),
    "hug": {"fore": to(1.14, 5.16), "aft": to(0.9, 4.98)},
    "tip": {"fore": to(0.5, 7.36, "out", frame="head"), "aft": to(-0.5, 7.36, "out", frame="head")},
    "mouth": {"fore": to(0.12, 6.38, frame="head", elbow=(0.06, -0.6)),     # (a hand at his lips, its elbow at his side, as her
              "aft": to(0.08, 6.32, frame="head", elbow=(0.06, -0.6))},     # salute: for a man, (elbow, hand) pairs ((0.06, -0.6),
                                                                              # (-0.8, 0.62)) and ((0.06, -0.6), (-1.0, 0.56)); the hand
                                                                              # in his head's frame, so a boy's reaches his lips too)
    "pocket": {"fore": to(0.74, 4.02, "out", hand=False), "aft": to(-0.74, 4.02, "out", hand=False)},
    "carry": ((0.52, -0.92), (0.16, 0.18)),
    "tray": ((0.56, -0.94), (0.7, 0.14)),
    "rest": ((0.34, -1.12), (0.64, -2.08)),
    "fold": {"fore": to(-0.42, 5.02, "out"), "aft": to(0.46, 5.2, "out")},
})

# ---- builds: a grown man's, and a boy's (heads, in his own frame before his size) ------------------------------------
BUILDS = {
    "man": {"shoulder": (0.92, 5.76), "neck": 0.21, "hand": (0.175, 0.145),
            "torso": [(-0.76, 4.45), (-0.76, 4.45), (-0.78, 4.9), (-0.85, 5.3), (-0.98, 5.52), (-1.18, 5.74), (-1.15, 5.94), (-1.0, 6.04),
                      (-0.78, 6.09), (-0.26, 6.14), (0.26, 6.14), (0.78, 6.09), (1.0, 6.04), (1.15, 5.94), (1.18, 5.74), (0.98, 5.52),
                      (0.85, 5.3), (0.78, 4.9), (0.76, 4.45), (0.76, 4.45)],
            "trouser": [0.5, 0.45, 0.37, 0.35, 0.33], "leg": [0.36, 0.3, 0.19, 0.22, 0.13],
            "sleeve": [0.3, 0.27, 0.23, 0.215, 0.185], "arm": [0.25, 0.21, 0.165, 0.18, 0.12],
            "head": [(0.0, 7.43), (0.3, 7.37), (0.44, 7.14), (0.46, 6.84), (0.42, 6.54), (0.3, 6.37), (0.12, 6.29),
                     (-0.12, 6.29), (-0.3, 6.37), (-0.42, 6.54), (-0.46, 6.84), (-0.44, 7.14), (-0.3, 7.37)]},
    "kid": {"shoulder": (0.84, 5.76), "neck": 0.17, "hand": (0.19, 0.16),
            "torso": [(-0.76, 4.4), (-0.76, 4.4), (-0.77, 4.9), (-0.8, 5.3), (-0.9, 5.52), (-1.1, 5.74), (-1.07, 5.93), (-0.92, 6.03),
                      (-0.7, 6.08), (-0.26, 6.12), (0.26, 6.12), (0.7, 6.08), (0.92, 6.03), (1.07, 5.93), (1.1, 5.74), (0.9, 5.52),
                      (0.8, 5.3), (0.77, 4.9), (0.76, 4.4), (0.76, 4.4)],
            "trouser": [0.5, 0.46, 0.38, 0.35, 0.33], "leg": [0.34, 0.27, 0.2, 0.22, 0.14],
            "sleeve": [0.3, 0.27, 0.23, 0.22, 0.19], "arm": [0.24, 0.2, 0.17, 0.18, 0.13],
            "head": [(0.0, 7.44), (0.32, 7.38), (0.47, 7.14), (0.49, 6.86), (0.45, 6.6), (0.33, 6.42), (0.14, 6.34),
                     (-0.14, 6.34), (-0.33, 6.42), (-0.45, 6.6), (-0.49, 6.86), (-0.47, 7.14), (-0.32, 7.38)]},
}


# ---- his frame --------------------------------------------------------------------------------------------------------
class Body(Fig):
    """A man's or a boy's frame on the grid: his feet at (x, y), `h` tall, facing s (+1 to the right), `wide` his build.
    `c` is his body's size beside his head's (a boy's is smaller), `drop` how far his hips sink (a crouch), `tilt` how
    far his body leans toward the side he faces (degrees, about his hips)."""

    def __init__(self, d, x, y, h, s=1, wide=1.0, heads=HEADS, c=1.0, drop=0.0, tilt=0.0, hw=1.0):
        Fig.__init__(self, d, x, y, h, s, wide)
        self.k = h / heads
        self.c, self.drop, self.tilt, self.hw = c, drop, tilt, hw
        self.hand, self.grips = {}, {}

    def _G(self, U, V):
        return (self.x + self.s * U * self.k, self.y + V * self.k)

    def _turn(self, du, dv, sign=1.0):
        a = math.radians(-self.tilt * sign)
        return (du * math.cos(a) - dv * math.sin(a), du * math.sin(a) + dv * math.cos(a))

    def up(self, u, v):
        """His body's frame to his figure's (heads from his feet)."""
        pv = (3.8 - self.drop) * self.c
        du, dv = self._turn(u * self.wide * self.c, (v - self.drop) * self.c - pv)
        return (du, pv + dv)

    def hd(self, u, v):
        """His head's frame to his figure's: his head as big as a man's, set on his neck."""
        nu, nv = self.up(0.0, NECK)
        du, dv = self._turn(u * self.hw, v - NECK)
        return (nu + du, nv + dv)

    def to_body(self, U, V):
        """A point of his figure's frame back to his body's (to send a hand there)."""
        pv = (3.8 - self.drop) * self.c
        du, dv = self._turn(U, V - pv, -1.0)
        return (du / (self.wide * self.c), (dv + pv) / self.c + self.drop)

    def P(self, u, v):
        return self._G(*self.up(u, v))

    def PL(self, u, v):
        return self._G(u * self.wide * self.c, v * self.c)

    def PH(self, u, v):
        return self._G(*self.hd(u, v))

    def Ps(self, pts):
        return [self.P(u, v) for u, v in pts]

    def PLs(self, pts):
        return [self.PL(u, v) for u, v in pts]

    def PHs(self, pts):
        return [self.PH(u, v) for u, v in pts]

    def r(self, v):
        return v * self.k * self.c

    def rh(self, v):
        return v * self.k

    def grip(self, side="fore"):
        """His hand painted again, over a thing it holds (a bat's handle, a rag, a cane): call it after the thing."""
        g = self.grips.get(side)
        if g:
            paint, (px, py), rx, ry, ang, ink = g
            self.d.oval(paint, px, py, rx, ry, turn=ang, ink=ink)


def _reach(S, T, bend, sx):
    """Where his elbow goes for his hand to reach T from his shoulder S (both in his body's frame)."""
    dx, dy = T[0] - S[0], T[1] - S[1]
    L = math.hypot(dx, dy) or 1e-6
    ux, uy = dx / L, dy / L
    L2 = min(max(L, abs(UPPER - FORE) + 0.05), UPPER + FORE - 0.03)
    H = (S[0] + ux * L2, S[1] + uy * L2)
    p = (UPPER * UPPER - FORE * FORE + L2 * L2) / (2 * L2)
    q = math.sqrt(max(UPPER * UPPER - p * p, 0.0))
    nx, ny = -uy, ux
    E1 = (S[0] + ux * p + nx * q, S[1] + uy * p + ny * q)
    E2 = (S[0] + ux * p - nx * q, S[1] + uy * p - ny * q)
    key = {"down": lambda e: e[1], "up": lambda e: -e[1], "out": lambda e: -sx * e[0], "in": lambda e: sx * e[0]}[bend]
    return min((E1, E2), key=key), H


def _sleeve(S, E, a, b, r0, r1, cap=True, n=10):
    """A sleeve's outline round his upper arm S-E, from a to b of its length: r0 wide where it starts (round over his
    shoulder when `cap`), r1 at its hem, which is straight across."""
    dx, dy = E[0] - S[0], E[1] - S[1]
    L = math.hypot(dx, dy) or 1.0
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    A = (S[0] + ux * L * a, S[1] + uy * L * a)
    Z = (S[0] + ux * L * b, S[1] + uy * L * b)
    pts = [(Z[0] + nx * r1, Z[1] + ny * r1), (A[0] + nx * r0, A[1] + ny * r0)]
    if cap:
        for i in range(1, n):
            t = math.pi * i / n
            pts.append((A[0] + r0 * (math.cos(t) * nx - math.sin(t) * ux), A[1] + r0 * (math.cos(t) * ny - math.sin(t) * uy)))
    return pts + [(A[0] - nx * r0, A[1] - ny * r0), (Z[0] - nx * r1, Z[1] - ny * r1)]


def _closed(points, steps=6):
    """A closed curve through the points, as fill() draws it; a point given twice stays a sharp corner."""
    out = []
    for p in smooth(points, True, steps):
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > 1e-3:
            out.append(p)
    if len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) < 1e-3:
        out.pop()
    return out


def _clip(poly, axis, lo, hi):
    """The part of a polygon between lo and hi along axis 0 (u) or 1 (v): a stripe of it."""
    def cut(pts, keep, val):
        out = []
        for i in range(len(pts)):
            a, b = pts[i - 1], pts[i]
            ka, kb = keep(a), keep(b)
            if ka != kb:
                t = (val - a[axis]) / (b[axis] - a[axis])
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
            if kb:
                out.append(b)
        return out
    pts = cut(poly, lambda p: p[axis] >= lo, lo)
    return cut(pts, lambda p: p[axis] <= hi, hi) if len(pts) > 2 else []


_BANDS_H = tuple((round(4.62 + 0.38 * k, 2), round(4.84 + 0.38 * k, 2)) for k in range(-7, 4))   # (stripes round him, h)


def _sleeve_shape(d, F, paint, outline, w, stripes=None, direction="v", S=None, E=None):
    """A short or rolled sleeve (its outline in his body's frame, round his upper arm S-E) and, when striped, its stripes
    as his body's run: down his arm (v) or round it (h, in step with his shirt's); its edge over their ends."""
    if not stripes:
        d.shape(paint, F.Ps(outline), ink=w)
        return
    d.shape(paint, F.Ps(outline), ink=0.0)
    dx, dy = E[0] - S[0], E[1] - S[1]
    L = math.hypot(dx, dy) or 1.0
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    loc = [((p[0] - S[0]) * ux + (p[1] - S[1]) * uy, (p[0] - S[0]) * nx + (p[1] - S[1]) * ny) for p in outline]
    if direction == "h":
        bands, axis = [(0.16 + 0.38 * k, 0.38 + 0.38 * k) for k in range(-2, int(L / 0.38) + 3)], 0
    else:
        bands, axis = [(-0.1625, -0.0775), (0.0775, 0.1625)], 1
    for lo, hi in bands:
        piece = _clip(loc, axis, lo, hi)
        if len(piece) > 2:
            d.shape(stripes, F.Ps([(S[0] + a * ux + b * nx, S[1] + a * uy + b * ny) for a, b in piece]), ink=0.0)
    d.shape(None, F.Ps(outline), ink=w)


def _rings(d, paint, G, radii, period, width, phase):
    """Stripes round a limb (a long sleeve: its chain G and radii on the grid), `width` wide every `period` along it,
    the first starting `phase` from its near end; painted between the limb's fill and its ink."""
    L = _limb_parts(G, radii)
    c, tang, rr = L["c"], L["tang"], L["rr"]
    acc = [0.0]
    for i in range(1, len(c)):
        acc.append(acc[-1] + math.hypot(c[i][0] - c[i - 1][0], c[i][1] - c[i - 1][1]))

    def at(sv):                                                  # (the centre, its normal and the radius, sv along it)
        j = max(0, min(len(c) - 2, next((k for k in range(1, len(c)) if acc[k] >= sv), len(c) - 1) - 1))
        seg = (acc[j + 1] - acc[j]) or 1.0
        f = max(0.0, min(1.0, (sv - acc[j]) / seg))
        px, py = c[j][0] + (c[j + 1][0] - c[j][0]) * f, c[j][1] + (c[j + 1][1] - c[j][1]) * f
        tx, ty = tang[j][0] * (1 - f) + tang[j + 1][0] * f, tang[j][1] * (1 - f) + tang[j + 1][1] * f
        tl = math.hypot(tx, ty) or 1.0
        r = rr[j] * (1 - f) + rr[j + 1] * f
        return (px, py), (-ty / tl, tx / tl), r

    s0 = phase
    while s0 < acc[-1]:
        s1 = min(s0 + width, acc[-1])
        if s1 > max(s0, 0.0):
            ss = [max(s0, 0.0)] + [a for a in acc if max(s0, 0.0) < a < s1] + [s1]
            left, right = [], []
            for sv in ss:
                (px, py), (nx, ny), r = at(sv)
                left.append((px + nx * r, py + ny * r))
                right.append((px - nx * r, py - ny * r))
            d.shape(paint, left + list(reversed(right)), ink=0.0)
        s0 += period


def _side_line(chain, off):
    """The chain moved `off` to its left (a stripe down a sleeve)."""
    out = []
    for i, p in enumerate(chain):
        a, b = chain[max(i - 1, 0)], chain[min(i + 1, len(chain) - 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(tx, ty) or 1.0
        out.append((p[0] - ty / ln * off, p[1] + tx / ln * off))
    return out


def _limb_parts(pts, radii, steps=5):
    """noseart's limb() outline taken apart, point for point: its left side and right side (each from the near end),
    its far end's and its near end's round caps, its centre line and its tangents."""
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
        a, b = c[max(i - 1, 0)], c[min(i + 1, m - 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(tx, ty)
        tx, ty = (tx / ln, ty / ln) if ln > 1e-9 else (1.0, 0.0)
        left.append((c[i][0] - ty * rr[i], c[i][1] + tx * rr[i]))
        right.append((c[i][0] + ty * rr[i], c[i][1] - tx * rr[i]))
        tang.append((tx, ty))

    def untangle(side):
        keep = [side[0]]
        for i in range(1, m):
            if (side[i][0] - keep[-1][0]) * tang[i][0] + (side[i][1] - keep[-1][1]) * tang[i][1] > -1e-6:
                keep.append(side[i])
        return keep

    (t0x, t0y), (t1x, t1y) = tang[0], tang[-1]
    a_end, a_start = math.atan2(t1x, -t1y), math.atan2(-t0x, t0y)
    end = [(c[-1][0] + rr[-1] * math.cos(a_end - math.pi * k / 8), c[-1][1] + rr[-1] * math.sin(a_end - math.pi * k / 8)) for k in range(1, 8)]
    start = [(c[0][0] + rr[0] * math.cos(a_start - math.pi * k / 8), c[0][1] + rr[0] * math.sin(a_start - math.pi * k / 8)) for k in range(1, 8)]
    return {"left": untangle(left), "right": untangle(right), "end": end, "start": start, "c": c, "tang": tang, "rr": rr}


def _hull(points):
    """The convex hull of the points, counter-clockwise."""
    pts = sorted(set((round(x, 6), round(y, 6)) for x, y in points))
    if len(pts) < 3:
        return pts

    def turn(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for q in pts:
        while len(lower) >= 2 and turn(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(pts):
        while len(upper) >= 2 and turn(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    return lower[:-1] + upper[:-1]


def _inside(p, poly):
    """Whether point p is inside the polygon."""
    x, y = p
    inside = False
    for i in range(len(poly)):
        (x0, y0), (x1, y1) = poly[i - 1], poly[i]
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            inside = not inside
    return inside


def _crossings(a, b):
    """Where two polylines cross each other."""
    out = []
    for i in range(len(a) - 1):
        (px, py), (qx, qy) = a[i], a[i + 1]
        rx, ry = qx - px, qy - py
        for j in range(len(b) - 1):
            (sx0, sy0), (sx1, sy1) = b[j], b[j + 1]
            ex, ey = sx1 - sx0, sy1 - sy0
            den = rx * ey - ry * ex
            if abs(den) < 1e-12:
                continue
            t = ((sx0 - px) * ey - (sy0 - py) * ex) / den
            u = ((sx0 - px) * ry - (sy0 - py) * rx) / den
            if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
                out.append((px + t * rx, py + t * ry))
    return out


def _ink_shown(d, outline, w, hidden):
    """The painter's edge round a closed outline (counter-clockwise), but only along the parts of it that lie outside
    every polygon in `hidden`: where it runs over a shape it was painted on to join (a trouser leg under his trousers'
    top), it leaves no seam."""
    step = max(d.ink * 0.35, 0.05)
    samples = []
    for i in range(len(outline)):
        (px, py), (qx, qy) = outline[i], outline[(i + 1) % len(outline)]
        m = max(1, int(math.hypot(qx - px, qy - py) / step))
        samples += [(px + (qx - px) * j / m, py + (qy - py) * j / m) for j in range(m)]
    shown = [not any(_inside(q, h) for h in hidden) for q in samples]
    if all(shown):
        d.shape(None, outline, ink=w)
        return
    k = shown.index(False)                                         # (start where it is hidden: every run then has both its ends)
    samples, shown = samples[k:] + samples[:k], shown[k:] + shown[:k]
    N, i = len(samples), 1
    while i < N:
        if not shown[i]:
            i += 1
            continue
        j = i
        while j < N and shown[j]:
            j += 1
        run = samples[i - 1:j] + [samples[j % N]]                   # (a point past each end, into the shape it meets)
        d._put("art_ink", d._edge(run, w, closed=False)[:len(run) - 1])  # (its stroke only: no tick past a corner here)
        i = j


def _flat_limb(d, paint, pts, radii, w):
    """A limb (a trouser leg, a short's) with its near end cut flat across and no ink along that cut: painted under a
    shape that covers its top (his trousers' top), the two join without a seam. Returns limb()'s parts and `ring`, its
    outline (counter-clockwise, from one corner of the flat top round to the other)."""
    L = _limb_parts(pts, radii)
    ring = ccw(L["left"] + L["end"] + list(reversed(L["right"])))
    d.shape(paint, ring, ink=0.0)
    d._put("art_ink", d._edge(ring, w, closed=False))
    L["ring"] = ring
    return L


_SEAT = [(-0.82, 4.3), (-0.82, 3.8), (-0.7, 3.44), (-0.46, 3.24), (-0.14, 3.17), (0.2, 3.2), (0.6, 3.36), (0.9, 3.7), (0.86, 4.2)]


def _trouser_top(d, F, paint, seated, lo, legs, ink):
    """His trousers' top, from his waist over his legs' tops: one straight-sided shape (so that no curve bulges past its
    ink), in his body's frame (it leans and sinks with him). `legs` are what his trouser legs left (_flat_limb()'s parts,
    painted already, their tops flat and uninked): it reaches over their tops, and its edge is inked only where it shows,
    down his sides to where his legs' own edges go on, and across his crotch between his legs. With legs=None (his legs
    painted after it, in front of him, or not at all) its whole edge is inked."""
    base = [(-0.77, 4.72), (0.77, 4.72)]
    if seated:
        base += smooth(_SEAT, False, 4)                            # (his seat: round, though the shape is straight-sided)
    elif not legs:
        base += [(-0.84, 4.2), (-0.92, 3.86), (-0.72, lo + 0.08), (0.0, lo), (0.72, lo + 0.08), (0.92, 3.86), (0.84, 4.2)]
    pts = F.Ps(base)
    if legs:
        dip = max(F.r(0.06), d.ink * 0.5)                         # (into each leg past its flat top: no hairline between them)
        for L in legs:
            (tx, ty) = L["tang"][0]
            for a, b in ((L["left"][0], L["right"][0]), (L["right"][0], L["left"][0])):
                pts.append(a)
                pts.append((a[0] + tx * dip + (b[0] - a[0]) * 0.06, a[1] + ty * dip + (b[1] - a[1]) * 0.06))
        if not seated:                                             # (his crotch: where his legs part, or his trousers' own)
            cx, cy = F.P(0.0, lo)
            if len(legs) == 2:
                inner = []
                for L in legs:
                    l0, r0 = L["left"][0], L["right"][0]
                    inner.append(L["left"] if abs(l0[0] - F.x) < abs(r0[0] - F.x) else L["right"])
                meet = _crossings(inner[0], inner[1])
                if meet:
                    mx, my = max(meet, key=lambda q: q[1])
                    if my - F.r(0.03) > cy:
                        cx, cy = mx, my - F.r(0.03)
            pts.append((cx, cy))
    hull = _hull(pts)
    if not legs:
        d.shape(paint, hull, ink=ink)
        return hull
    d.shape(paint, hull, ink=0.0)
    _ink_shown(d, hull, ink, [L["ring"] for L in legs])
    return hull


def _band(d, paint, centre, half, w, within=None, reach=(False, False)):
    """A strap of even width (`half` each side of its centre line, a list of points on the grid) with the painter's
    edge along its sides. An end in `reach` (start, end) runs on to the edge of `within` (a closed outline: the head or
    the helmet it goes round) and is cut along that edge, so that the strap goes round out of sight there and nothing
    of it shows past the edge; any other end is cut square, and inked."""
    c = [tuple(p) for p in centre]
    if within:
        for k, (i0, i1) in enumerate(((1, 0), (-2, -1))):        # (the reaching ends carried on past the edge)
            if reach[k]:
                (ax, ay), (bx, by) = c[i0], c[i1]
                ln = math.hypot(bx - ax, by - ay) or 1.0
                far = (bx + (bx - ax) / ln * half * 6, by + (by - ay) / ln * half * 6)
                c = [far] + c if k == 0 else c + [far]
    n = len(c)
    left, right = [], []
    for i in range(n):
        (ax, ay), (bx, by) = c[max(i - 1, 0)], c[min(i + 1, n - 1)]
        ln = math.hypot(bx - ax, by - ay) or 1.0
        nx, ny = -(by - ay) / ln, (bx - ax) / ln
        left.append((c[i][0] + nx * half, c[i][1] + ny * half))
        right.append((c[i][0] - nx * half, c[i][1] - ny * half))
    mid = n // 2

    def cut(side, step):                                         # (a side from the middle out to where it leaves `within`)
        out, i = [side[mid]], mid
        while 0 <= i + step < n:
            a, b = side[i], side[i + step]
            if within and reach[0 if step < 0 else 1]:
                hits = []
                for j in range(len(within)):
                    q = _crossings([a, b], [within[j - 1], within[j]])
                    hits += [(math.hypot(h[0] - a[0], h[1] - a[1]), h, j) for h in q]
                if hits:
                    _, h, j = min(hits)
                    out.append(h)
                    return out, j
            out.append(b)
            i += step
        return out, None

    (lb, jlb), (rb, jrb) = cut(left, -1), cut(right, -1)
    (lf, jlf), (rf, jrf) = cut(left, 1), cut(right, 1)
    if (jlf is None) != (jrf is None):                            # (one side only past the edge: both cut square, short of it)
        k = min(len(lf), len(rf)) - 1
        lf, rf, jlf, jrf = lf[:k], rf[:k], None, None
    if (jlb is None) != (jrb is None):
        k = min(len(lb), len(rb)) - 1
        lb, rb, jlb, jrb = lb[:k], rb[:k], None, None
    L = list(reversed(lb)) + lf[1:]                               # (each side, start to end)
    R = list(reversed(rb)) + rf[1:]

    def arc(j0, p0, j1, p1):                                     # (along `within` from p0 (on its edge j0) to p1 (on j1))
        m = len(within)
        fwd = [within[(j0 + t) % m] for t in range((j1 - j0) % m)]
        back = [within[(j0 - 1 - t) % m] for t in range((j0 - j1) % m)]
        return [p0] + (fwd if len(fwd) <= len(back) else back) + [p1]

    cut_end = jlf is not None and jrf is not None
    cut_start = jlb is not None and jrb is not None
    end = arc(jlf, L[-1], jrf, R[-1])[1:-1] if cut_end else []
    start = arc(jrb, R[0], jlb, L[0])[1:-1] if cut_start else []
    d.shape(paint, L + end + list(reversed(R)) + start, ink=0.0)
    if not w:
        return
    if cut_start and cut_end:
        paths = [L, R]
    elif cut_end:
        paths = [list(reversed(R)) + L]
    elif cut_start:
        paths = [L + list(reversed(R))]
    else:
        paths = [L + list(reversed(R)) + [L[0]]]
    for p in paths:
        d._put("art_ink", d._edge(p, w, closed=False)[:len(p) - 1])


# ---- the leather flying helmet ------------------------------------------------------------------------------------------
_HELMET = {   # (its leather cap, the opening its face shows through, its lenses, its chin strap; a head 0.94 wide, 1.14 tall)
    "front": {"cap": [(-0.44, -0.5), (-0.53, -0.25), (-0.56, 0.05), (-0.54, 0.32), (-0.44, 0.54), (-0.24, 0.68), (0.0, 0.72),
                      (0.24, 0.68), (0.44, 0.54), (0.54, 0.32), (0.56, 0.05), (0.53, -0.25), (0.44, -0.5), (0.36, -0.52),
                      (0.38, -0.2), (0.42, 0.1), (0.37, 0.32), (0.2, 0.41), (0.04, 0.43), (-0.14, 0.41), (-0.32, 0.32),
                      (-0.36, 0.1), (-0.32, -0.2), (-0.3, -0.52)],
              "open": (0.04, -0.06, 0.4, 0.5), "lenses": [(-0.18, 0.47, 0.2, 0.155), (0.27, 0.47, 0.2, 0.155)],
              "band": [(-0.55, 0.38), (-0.3, 0.46), (0.04, 0.48), (0.34, 0.46), (0.56, 0.38)],
              "strap": [(-0.36, -0.5), (-0.1, -0.64), (0.2, -0.64), (0.4, -0.5)], "cups": [(-0.47, -0.1), (0.5, -0.1)],
              "seam": [(0.04, 0.72), (0.04, 0.64)]},
    "three": {"cap": [(-0.06, -0.52), (-0.3, -0.55), (-0.48, -0.3), (-0.57, 0.0), (-0.54, 0.32), (-0.4, 0.56), (-0.16, 0.7),
                      (0.1, 0.72), (0.34, 0.62), (0.5, 0.42), (0.56, 0.2), (0.5, 0.14), (0.34, 0.32), (0.13, 0.38),
                      (-0.04, 0.32), (-0.12, 0.16), (-0.08, -0.1), (-0.03, -0.36)],
              "far": [(0.5, 0.16), (0.57, -0.12), (0.52, -0.42), (0.42, -0.52), (0.36, -0.4), (0.42, -0.12), (0.44, 0.1)],
              "open": (0.22, -0.1, 0.33, 0.47), "lenses": [(0.06, 0.5, 0.155, 0.125), (0.37, 0.46, 0.105, 0.115)],
              "band": [(-0.08, 0.5), (-0.32, 0.46), (-0.56, 0.32)],
              "strap": [(-0.05, -0.5), (0.14, -0.63), (0.38, -0.6)], "cups": [(-0.27, -0.12)],
              "seam": [(-0.2, 0.68), (-0.36, 0.56), (-0.5, 0.36)]},
    "side": {"cap": [(0.08, -0.5), (-0.2, -0.55), (-0.42, -0.36), (-0.56, -0.05), (-0.52, 0.28), (-0.34, 0.55), (-0.06, 0.69),
                     (0.22, 0.63), (0.4, 0.45), (0.49, 0.24), (0.4, 0.15), (0.24, 0.1), (0.13, 0.0), (0.1, -0.24), (0.13, -0.44)],
             "open": None, "lenses": [(0.36, 0.42, 0.085, 0.135)],
             "band": [(0.3, 0.42), (0.0, 0.46), (-0.3, 0.42), (-0.55, 0.26)],
             "strap": [(0.1, -0.48), (0.3, -0.62), (0.5, -0.6)], "cups": [(-0.18, -0.12)],
             "seam": [(-0.06, 0.69), (-0.2, 0.62), (-0.36, 0.5)]},
}


def helmet(d, x, y, size, s=1, paint="art_brown", goggles=True, strap=True, view="three", part="all", inside="art_dark",
           turn=0.0, ink=None, lens="art_sky", rim="art_lsteel", wide=1.0):
    """A leather flying helmet with its goggles pushed up on the brow, as the crews wore them. (x, y) is the middle of
    the head it fits and `size` that head's width; it faces s. view: three (three-quarter, the default), side (a
    profile, for a beast's long head) or front (as man() wears it). Alone, an empty helmet, its face's opening shows
    `inside` (its dark lining): part="inside" paints only that and part="cap" all the rest, so that a creature can be
    painted between them, peeking out; on a head painted first, give inside=None. The goggles' band goes round to the
    leather's edge and is cut along it (it goes on out of sight); the chin strap is the helmet's own leather; both are
    edged finely. Returns its points on the grid: face (the middle of its opening), top, brow, lenses (their middles),
    chin (where its strap goes under)."""
    g = _HELMET[view]
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        u, v = u * size * wide, v * size
        return (x + s * (u * c - v * sn), y + u * sn + v * c)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    w0 = d.ink * max(0.5, min(1.0, size / 16.0)) if ink is None else ink
    if part in ("all", "inside") and inside and g["open"]:
        ou, ov, orx, ory = g["open"]
        d.fill(inside, Q(ellipse(ou, ov, orx, ory, 0, 24)), ink=w0)
    if part in ("all", "cap"):
        if inside and g.get("far"):
            d.fill(paint, Q(g["far"]), ink=w0 * 0.9, steps=4)
        cap = d.fill(paint, Q(g["cap"]), ink=w0, steps=4)
        d.line("art_ink", Q(g["seam"]), size * 0.03)
        for cu, cv in g["cups"]:                                 # (the earphone's cup under the leather)
            d.oval(paint, *P(cu, cv), size * 0.1 * wide, size * 0.15, turn=turn, ink=w0 * 0.75)
            d.oval("art_ink", *P(cu, cv), size * 0.03, size * 0.045, turn=turn, ink=0.0)
        if strap:                                                 # (in its own leather, edged as the band is)
            st = Q(g["strap"])
            _band(d, paint, smooth(st, False, 5) if len(st) > 2 else st, size * 0.035, w0 * 0.5)
            (u0, v0), (u1, v1) = g["strap"][-2], g["strap"][-1]       # (its buckle, along the strap's end)
            along = math.degrees(math.atan2(v1 - v0, s * (u1 - u0) * wide)) + turn
            d.oval(rim, *P(u1, v1), size * 0.05, size * 0.032, turn=along, ink=w0 * 0.5)
        if goggles:                                               # (the band round to the helmet's edge, and no further)
            reach = {"front": (True, True), "three": (False, True), "side": (False, True)}[view]
            _band(d, "art_black", smooth(Q(g["band"]), False, 5), size * 0.05, w0 * 0.5, within=_hull(cap), reach=reach)
            ls = g["lenses"]
            if len(ls) == 2:
                d.line("art_black", [P(ls[0][0] + ls[0][2] * 0.7, ls[0][1]), P(ls[1][0] - ls[1][2] * 0.7, ls[1][1])], size * 0.08, taper=False)
            for lu, lv, lrx, lry in reversed(ls):
                d.oval(rim, *P(lu, lv), lrx * size * wide, lry * size, turn=turn, ink=w0 * 0.9)
                d.oval(lens, *P(lu, lv), lrx * size * wide * 0.68, lry * size * 0.68, turn=turn, ink=w0 * 0.45)
                d.line("art_white", [P(lu - lrx * 0.35, lv + lry * 0.2), P(lu - lrx * 0.05, lv + lry * 0.38)], size * 0.035)
    ou, ov = (g["open"] or (0.3, -0.1, 0, 0))[:2]
    return {"face": P(ou, ov), "top": P(0.0, 0.7), "brow": P(*g["band"][len(g["band"]) // 2]),
            "lenses": [P(lu, lv) for lu, lv, _, _ in g["lenses"]], "chin": P(*g["strap"][len(g["strap"]) // 2])}


# ---- an airman's long scarf, on any figure ------------------------------------------------------------------------------
def scarf(d, F, paint="art_white", part="all", tail="aft", length=2.6, rise=0.45, waves=1.5, ink=None):
    """An airman's long silk scarf on any figure's frame: a man's or a boy's (man(), kid(): wear={"scarf": paint} puts it
    on him), or lady()'s (call it after her: scarf(d, F, "art_white")). part="wrap" paints it wound round the neck,
    one end tucked down the chest; part="tail" its long end, streaming from the nape: paint it after the arms (man()
    and kid() do), so that nothing covers it; part="all" both. tail="aft" streams it out behind (the wind in the face,
    flying); tail="ahead" streams it out before the face (the wind at the back). `length` is heads from the nape to its
    end (2 to 3), `rise` how far it climbs by then, `waves` how many waves it rises in (1 or 2), before it splits in
    two, each end fringed. Its edge is fine. Returns the tail's outline (or the wrap's) on the grid."""
    PN = getattr(F, "PH", F.P)                                   # (a man's head's frame; lady()'s one frame)
    RN = getattr(F, "rh", F.r)
    nk = getattr(F, "neck", 0.18)                                # (the neck's half width, heads)
    w0 = d.ink * 0.7 if ink is None else ink
    wrap = [(-(nk + 0.13), 6.03), (-(nk + 0.1), 6.15), (0.0, 6.1), (nk + 0.1, 6.15), (nk + 0.13, 6.03), (nk + 0.11, 5.93),
            (0.0, 5.88), (-(nk + 0.11), 5.93)]
    if part in ("all", "wrap"):
        edge = d.fill(paint, [PN(u, v) for u, v in wrap], ink=w0, steps=4)
        d.fill(paint, F.Ps([(0.02, 6.0), (0.2, 5.98), (0.24, 5.4), (0.1, 5.3), (0.02, 5.44)]), ink=w0, steps=3)
        if part == "wrap":
            return edge
    sd = -1.0 if tail == "aft" else 1.0                          # (the way it streams, in his frame)
    n, ts = 28, 0.8                                              # (its points; where it splits)
    ru, rv = sd * (nk + 0.02), 6.03
    C = []
    for i in range(n + 1):
        t = i / n
        v = rv + rise * t + 0.12 * math.sin(2 * math.pi * waves * t) * min(1.0, 4 * t)
        C.append(PN(ru + sd * length * t, v))
    N = []
    for i in range(n + 1):
        (ax, ay), (bx, by) = C[max(i - 1, 0)], C[min(i + 1, n)]
        ln = math.hypot(bx - ax, by - ay) or 1.0
        N.append((-(by - ay) / ln, (bx - ax) / ln))
    tx, ty = N[-1][1], -N[-1][0]                                 # (along it, at its end)
    k = int(round(ts * n))
    hw = [RN(0.15 - 0.03 * i / n) for i in range(n + 1)]
    h2 = hw[k] / 2.0

    def off(i, o):
        return (C[i][0] + N[i][0] * o, C[i][1] + N[i][1] * o)

    def sep(i):                                                  # (each end's middle, out from the split)
        return h2 + RN(0.1) * (i - k) / (n - k)

    def fringe(sgn):                                             # (an end's fringe, from its outer corner in)
        out = []
        for j in range(9):
            o = sgn * (sep(n) + h2 - 2 * h2 * j / 8.0)
            dep = RN(0.13) if j % 2 else 0.0
            px, py = off(n, o)
            out.append((px + tx * dep, py + ty * dep))
        return out

    outline = ([off(i, hw[i]) for i in range(0, k + 1)] + [off(i, sep(i) + h2) for i in range(k + 1, n)] + fringe(1.0) +
               [off(i, sep(i) - h2) for i in range(n - 1, k, -1)] + [off(k, 0.0)] + [off(i, -(sep(i) - h2)) for i in range(k + 1, n)] +
               fringe(-1.0) + [off(i, -(sep(i) + h2)) for i in range(n - 1, k, -1)] + [off(i, -hw[i]) for i in range(k, -1, -1)])
    outline = ccw(outline)
    d.shape(paint, outline, ink=0.0)
    hidden = [smooth([PN(u, v) for u, v in wrap], True, 4)]       # (no edge where it comes out of the wrap, nor on its fringe)
    for sgn in (1.0, -1.0):
        a, b = off(n, sgn * (sep(n) + h2 * 1.3)), off(n, sgn * (sep(n) - h2 * 1.3))
        e = RN(0.3)
        hidden.append([(a[0] - tx * RN(0.004), a[1] - ty * RN(0.004)), (a[0] + tx * e, a[1] + ty * e), (b[0] + tx * e, b[1] + ty * e),
                       (b[0] - tx * RN(0.004), b[1] - ty * RN(0.004))])
        hem = [off(n, sgn * (sep(n) + h2)), off(n, sgn * (sep(n) - h2))]
        d.line("art_ink", hem, w0 * 0.8, taper=False)              # (its hem, across the end; the fringe beyond it bare)
    _ink_shown(d, outline, w0, hidden)
    return outline


# ---- his head: ears, face, hair, hat -------------------------------------------------------------------------------------
def _head(d, F, build, skin, hair, hair_paint, hat, w, smile, mouth, eyes, moustache, cheeks, fine):
    PH, PHs, RH = F.PH, F.PHs, F.rh
    fx = FACE_X
    kid = build == "kid"
    B = BUILDS[build]
    # (his ears, then his head over them)
    if hat not in ("flying",):
        for sx in (-1, 1):
            d.oval(skin, *PH(sx * (0.45 if not kid else 0.47), 6.83), RH(0.09), RH(0.14), turn=F.tilt * -F.s, ink=fine)
            d.line("art_ink", PHs([(sx * 0.49, 6.92), (sx * 0.515, 6.84), (sx * 0.49, 6.75)]), RH(0.028))
    F.head_edge = d.fill(skin, PHs(B["head"]), ink=fine, steps=5)       # (his head's outline: goggles' band stops at it)
    F.hair_edge = None
    # (his eyes, as hers without the lashes; his brows heavier, straighter)
    ey = 6.885 if not kid else 6.86
    for i, (ex, big) in enumerate(((-0.17 + fx, 0.86), (0.18 + fx, 1.0))):
        b = big * (1.12 if kid else 1.0)
        shut = eyes == "shut" or (eyes == "wink" and i == 1)
        if shut:
            d.line("art_ink", PHs([(ex - 0.11 * b, ey + 0.035), (ex, ey - 0.02), (ex + 0.11 * b, ey + 0.035)]), RH(0.045), taper=False)
        else:
            d.line("art_ink", PHs([(ex - 0.11 * b, ey + 0.045), (ex, ey + 0.1), (ex + 0.11 * b, ey + 0.065)]), RH(0.046), taper=False)
            d.oval("art_ink", *PH(ex + 0.01, ey + 0.005), RH(0.05 * b), RH(0.072 * (1.08 if kid else 1.0)), ink=0.0)
        o = -1.0 if i == 0 else 1.0                              # (toward his eye's outer corner)
        bw, bh = (0.11, 0.034) if kid else (0.13, 0.048)
        d.line("art_ink", PHs([(ex - o * bw * 0.9, 7.15), (ex + o * 0.01, 7.165), (ex + o * bw, 7.115)]), RH(bh))
    # (his nose)
    if kid:
        d.line("art_ink", PHs([(fx + 0.05, 6.77), (fx + 0.09, 6.71), (fx + 0.03, 6.69)]), RH(0.032), taper=False)
    else:
        d.line("art_ink", PHs([(fx + 0.06, 6.85), (fx + 0.125, 6.72), (fx + 0.03, 6.68)]), RH(0.038), taper=False)
    if cheeks:
        d.oval("art_pink", *PH(fx + 0.26, 6.64), RH(0.07 if kid else 0.06), RH(0.042 if kid else 0.035), ink=0.0)
        d.oval("art_pink", *PH(fx - 0.25, 6.64), RH(0.06 if kid else 0.05), RH(0.042 if kid else 0.035), ink=0.0)
    # (his mouth, and over it his moustache)
    my = 6.53 if not kid else 6.52
    if moustache:
        my -= 0.035
    m = mouth or ("smile" if smile else "flat")
    if m == "smile":
        d.line("art_ink", PHs([(fx - 0.15, my + 0.04), (fx, my - 0.01), (fx + 0.15, my + 0.045)]), RH(0.036), taper=False)
    elif m == "flat":
        d.line("art_ink", PHs([(fx - 0.11, my + 0.01), (fx + 0.11, my + 0.01)]), RH(0.034), taper=False)
    elif m == "open":
        d.fill("art_ink", PHs([(fx - 0.14, my + 0.04), (fx, my + 0.03), (fx + 0.14, my + 0.045), (fx + 0.09, my - 0.08), (fx, my - 0.115),
                               (fx - 0.09, my - 0.08)]), ink=0.0, steps=4)
        d.oval("art_red", *PH(fx + 0.01, my - 0.075), RH(0.065), RH(0.032), ink=0.0)
    elif m == "grin":
        d.fill("art_white", PHs([(fx - 0.16, my + 0.04), (fx, my + 0.015), (fx + 0.16, my + 0.045), (fx + 0.1, my - 0.05), (fx, my - 0.07),
                                 (fx - 0.1, my - 0.05)]), ink=fine * 0.6, steps=4)
        d.line("art_ink", PHs([(fx - 0.13, my + 0.005), (fx + 0.13, my + 0.01)]), RH(0.018), taper=False)
    elif m == "o":
        d.oval("art_ink", *PH(fx + 0.01, my - 0.01), RH(0.045), RH(0.055), ink=0.0)
    if moustache == "trim":
        d.fill(hair_paint, PHs([(fx - 0.16, 6.575), (fx - 0.06, 6.63), (fx, 6.615), (fx + 0.06, 6.63), (fx + 0.16, 6.575), (fx, 6.585)]), ink=fine * 0.6, steps=3)
    elif moustache == "full":
        d.fill(hair_paint, PHs([(fx - 0.24, 6.5), (fx - 0.15, 6.63), (fx, 6.66), (fx + 0.15, 6.63), (fx + 0.24, 6.5), (fx + 0.12, 6.56),
                                (fx, 6.58), (fx - 0.12, 6.56)]), ink=fine * 0.7, steps=3)
    # (his hair: under his hat, at its sides)
    if hat not in ("flying",) and hair not in (None, "none"):
        F.hair_edge = _hair(d, F, hair, hair_paint, fine, kid, hat)
    _hat(d, F, hat, w, fine, kid)
    F.face, F.mouth, F.top = PH(fx, 6.86), PH(fx, my), PH(0.0, 7.43)


_HAIR_CUT = {   # (his hair's outline in his head's frame, over the top from his aft side round to his fore, then back along
               # his hairline: a 1940s cut, its sides short above his ears, his forehead and temples showing, a side part
               # notched into it over his aft brow)
    "short": [(-0.47, 7.0), (-0.5, 7.2), (-0.45, 7.42), (-0.34, 7.54), (-0.24, 7.58), (-0.2, 7.47), (-0.14, 7.6), (0.04, 7.7),
              (0.22, 7.72), (0.37, 7.62), (0.47, 7.44), (0.49, 7.22), (0.48, 7.04), (0.44, 7.14), (0.4, 7.29), (0.3, 7.37),
              (0.1, 7.38), (-0.1, 7.38), (-0.27, 7.34), (-0.37, 7.25), (-0.43, 7.1)],          # (his forelock lifted over his fore brow)
    "slick": [(-0.47, 7.0), (-0.5, 7.2), (-0.45, 7.42), (-0.33, 7.53), (-0.24, 7.56), (-0.2, 7.48), (-0.14, 7.57), (0.08, 7.6),
              (0.3, 7.55), (0.44, 7.42), (0.49, 7.22), (0.48, 7.04), (0.44, 7.14), (0.4, 7.28), (0.3, 7.35), (0.1, 7.37),
              (-0.1, 7.37), (-0.27, 7.34), (-0.37, 7.25), (-0.43, 7.1)],                       # (combed flat and back)
}


def _hair(d, F, hair, paint, fine, kid, hat=None):
    """His hair, under any hat; returns its outline on the grid (None for none)."""
    PHs, RH = F.PHs, F.rh
    if hair == "bald":
        for sx in (-1, 1):
            d.fill(paint, PHs([(sx * 0.46, 6.86), (sx * 0.5, 7.08), (sx * 0.44, 7.24), (sx * 0.38, 7.04)]), ink=fine * 0.8, steps=3)
        return None
    if hair == "crew":
        edge = d.fill(paint, PHs([(-0.46, 6.9), (-0.49, 7.2), (-0.43, 7.46), (-0.2, 7.53), (0.2, 7.53), (0.43, 7.46), (0.49, 7.2), (0.46, 6.9),
                                  (0.42, 7.1), (0.3, 7.2), (0.0, 7.23), (-0.3, 7.2), (-0.42, 7.08)]), ink=fine, steps=4)
        for u in (-0.24, -0.08, 0.08, 0.24):
            d.line("art_ink", PHs([(u, 7.4), (u + 0.01, 7.5)]), RH(0.025), taper=False)
        return edge
    if hair == "tousle":                                         # (a boy's: a fringe in three points and a cowlick)
        pts = [(-0.48, 6.9), (-0.52, 7.14), (-0.46, 7.4), (-0.24, 7.56), (-0.1, 7.7), (-0.02, 7.58), (0.12, 7.6), (0.36, 7.5),
               (0.51, 7.28), (0.49, 6.94), (0.44, 7.1), (0.33, 7.12), (0.24, 7.22), (0.12, 7.1), (0.02, 7.2), (-0.1, 7.08),
               (-0.2, 7.2), (-0.36, 7.14), (-0.44, 7.02)]
        return d.fill(paint, PHs(pts), ink=fine, steps=4)
    cut = _HAIR_CUT["slick" if hair == "slick" else "short"]
    if hat and hat != "goggles":                                 # (under a hat's crown: his forelock pressed down)
        cut = [(u, min(v, 7.54)) for u, v in cut]
    wk = 1.04 if kid else 1.0                                    # (a boy's head a little wider)
    edge = d.fill(paint, PHs([(u * wk, v) for u, v in cut]), ink=fine, steps=4)
    d.line("art_ink", PHs([(-0.2 * wk, 7.5), (-0.22 * wk, 7.43)]), RH(0.028))       # (the part, down from its notch)
    if hair == "slick":
        d.line("art_lgrey", PHs([(-0.1 * wk, 7.5), (0.1 * wk, 7.53), (0.3 * wk, 7.47)]), RH(0.045))
    else:
        d.line("art_ink", PHs([(-0.1 * wk, 7.5), (0.12 * wk, 7.56), (0.32 * wk, 7.5)]), RH(0.026))   # (his forelock's wave)
    return edge


def _hat(d, F, hat, w, fine, kid):
    if not hat:
        return
    P, Ps, R = F.PH, F.PHs, F.rh
    fx = FACE_X
    col = w.get("cap") or w.get("hat")

    def bill(paint, lo=7.18, hi=7.3, out=0.84):            # (a cap's bill, out toward the side he faces)
        d.fill(paint, Ps([(-0.46, hi - 0.02), (-0.22, lo + 0.02), (0.2, lo - 0.02), (0.6, lo - 0.01), (out, lo + 0.04), (out - 0.06, hi - 0.01),
                          (0.4, hi + 0.01), (0.0, hi + 0.02), (-0.36, hi + 0.03)]), ink=fine, steps=4)

    if hat == "ball":
        col = col or "art_red"
        d.fill(col, Ps([(-0.51, 7.2), (-0.52, 7.42), (-0.38, 7.64), (-0.12, 7.75), (0.14, 7.75), (0.4, 7.64), (0.54, 7.42), (0.53, 7.2),
                        (0.3, 7.24), (0.0, 7.26), (-0.3, 7.24)]), ink=fine, steps=4)
        for u in (-0.2, 0.22):
            d.line("art_ink", Ps([(0.01, 7.74), (u * 0.6, 7.5), (u, 7.27)]), R(0.024))
        d.oval(col, *P(0.01, 7.77), R(0.06), R(0.045), ink=fine * 0.6)
        bill(w.get("bill", col), 7.17, 7.3, 0.9)
    elif hat == "paper":                                      # (folded white paper, a boat on his head)
        col = col or "art_white"
        d.shape(col, Ps([(-0.6, 7.2), (0.64, 7.2), (0.74, 7.7), (0.4, 7.74), (0.06, 7.64), (-0.28, 7.74), (-0.68, 7.68)]), ink=fine)
        d.shape(col, Ps([(-0.6, 7.2), (0.64, 7.2), (0.66, 7.4), (-0.62, 7.4)]), ink=fine * 0.8)
        d.line("art_lgrey", Ps([(0.06, 7.62), (0.08, 7.44)]), R(0.04))
    elif hat == "toque":
        col = col or "art_white"
        d.blobs(col, [P(-0.26, 7.82) + (R(0.3),), P(0.28, 7.84) + (R(0.31),), P(0.0, 8.06) + (R(0.34),)], ink=fine)
        d.fill(col, Ps([(-0.46, 7.18), (0.0, 7.22), (0.48, 7.18), (0.5, 7.5), (0.0, 7.54), (-0.48, 7.5)]), ink=fine, steps=2)
        for u in (-0.2, 0.06, 0.3):
            d.line("art_lgrey", Ps([(u, 7.56), (u + 0.02, 7.86)]), R(0.035))
    elif hat == "boater":
        col, band = col or "art_yellow", w.get("band", "art_navy")
        brim = ellipse(0.0, 7.3, 0.84, 0.11, 0, 28)
        d.fill(col, Ps([(u, v - 0.05 * u) for u, v in brim]), ink=fine, steps=2)
        crown = [(-0.42, 7.3), (-0.42, 7.68), (0.42, 7.68), (0.42, 7.3)]
        d.shape(col, Ps([(u, v - 0.05 * u) for u, v in crown]), ink=fine)
        d.shape(band, Ps([(u, v - 0.05 * u) for u, v in [(-0.42, 7.31), (-0.42, 7.45), (0.42, 7.45), (0.42, 7.31)]]), ink=fine * 0.7)
        d.line("art_gold", Ps([(u, v - 0.05 * u) for u, v in [(-0.3, 7.63), (0.0, 7.64), (0.3, 7.63)]]), R(0.03))
    elif hat == "garrison":
        col = col or "art_khaki"
        d.fill(col, Ps([(-0.56, 7.28), (0.2, 7.36), (0.56, 7.5), (0.46, 7.76), (-0.1, 7.82), (-0.52, 7.6)]), ink=fine, steps=3)
        d.line("art_ink", Ps([(-0.48, 7.44), (0.0, 7.5), (0.44, 7.6)]), R(0.03))
    elif hat == "fedora":
        col, band = col or "art_brown", w.get("band", "art_dark")
        d.fill(col, Ps([(-0.41, 7.3), (-0.44, 7.62), (-0.32, 7.84), (-0.08, 7.86), (0.0, 7.78), (0.08, 7.86), (0.32, 7.84), (0.44, 7.62),
                        (0.41, 7.3)]), ink=fine, steps=4)
        d.line("art_ink", Ps([(-0.18, 7.79), (0.0, 7.7), (0.18, 7.79)]), R(0.03))
        d.shape(band, Ps([(-0.42, 7.32), (-0.43, 7.46), (0.43, 7.46), (0.42, 7.32)]), ink=fine * 0.7)
        d.fill(col, Ps([(-0.86, 7.36), (-0.5, 7.27), (0.0, 7.22), (0.5, 7.27), (0.86, 7.36), (0.6, 7.42), (0.0, 7.4), (-0.6, 7.42)]), ink=fine, steps=4)
    elif hat in ("cap", "newsboy"):
        col = col or ("art_navy" if hat == "cap" else "art_grey")
        if hat == "cap":
            d.fill(col, Ps([(-0.5, 7.18), (-0.53, 7.46), (-0.36, 7.66), (0.0, 7.72), (0.38, 7.66), (0.55, 7.46), (0.52, 7.18), (0.0, 7.24)]), ink=fine, steps=4)
            d.line("art_ink", Ps([(-0.5, 7.32), (0.0, 7.37), (0.52, 7.32)]), R(0.03))
        else:
            d.fill(col, Ps([(-0.58, 7.2), (-0.64, 7.44), (-0.42, 7.66), (0.0, 7.73), (0.44, 7.68), (0.68, 7.48), (0.62, 7.2), (0.0, 7.26)]), ink=fine, steps=4)
            for u in (-0.3, 0.32):
                d.line("art_ink", Ps([(0.01, 7.72), (u * 0.7, 7.5), (u, 7.3)]), R(0.024))
            d.oval(col, *P(0.01, 7.74), R(0.06), R(0.045), ink=fine * 0.6)
        bill(col, 7.17, 7.29, 0.78)
    elif hat == "straw":
        col, band = col or "art_yellow", w.get("band", "art_red")
        d.fill(col, Ps([(-1.08, 7.28), (-0.6, 7.42), (0.0, 7.46), (0.6, 7.42), (1.08, 7.28), (0.6, 7.16), (0.0, 7.12), (-0.6, 7.16)]), ink=fine, steps=4)
        d.fill(col, Ps([(-0.47, 7.36), (-0.43, 7.76), (0.0, 7.86), (0.43, 7.76), (0.47, 7.36)]), ink=fine, steps=4)
        d.shape(band, Ps([(-0.47, 7.36), (-0.46, 7.5), (0.46, 7.5), (0.47, 7.36)]), ink=fine * 0.7)
    elif hat == "cowboy":
        col, band = col or "art_tan", w.get("band", "art_brown")
        d.fill(col, Ps([(-1.04, 7.5), (-0.76, 7.38), (0.0, 7.34), (0.76, 7.38), (1.04, 7.5), (0.88, 7.28), (0.0, 7.16), (-0.88, 7.28)]), ink=fine, steps=4)
        d.fill(col, Ps([(-0.47, 7.36), (-0.52, 7.88), (-0.2, 7.97), (0.0, 7.86), (0.2, 7.97), (0.52, 7.88), (0.47, 7.36)]), ink=fine, steps=4)
        d.shape(band, Ps([(-0.48, 7.38), (-0.49, 7.51), (0.49, 7.51), (0.48, 7.38)]), ink=fine * 0.7)
    elif hat == "peaked":
        col, band = col or "art_navy", w.get("band", "art_black")
        d.fill(col, Ps([(-0.5, 7.42), (-0.72, 7.62), (-0.54, 7.77), (0.0, 7.82), (0.54, 7.77), (0.72, 7.62), (0.5, 7.42)]), ink=fine, steps=4)
        d.shape(band, Ps([(-0.48, 7.24), (0.48, 7.24), (0.5, 7.46), (-0.5, 7.46)]), ink=fine)
        d.fill("art_black", Ps([(-0.46, 7.26), (-0.1, 7.15), (0.3, 7.14), (0.62, 7.2), (0.5, 7.28), (0.0, 7.28)]), ink=fine * 0.8, steps=4)
        d.oval(w.get("badge", "art_gold"), *P(0.02, 7.55), R(0.09), R(0.08), ink=fine * 0.6)
    elif hat == "derby":
        col = col or "art_black"
        d.fill(col, Ps([(-0.42, 7.3), (-0.44, 7.6), (-0.24, 7.8), (0.0, 7.84), (0.24, 7.8), (0.44, 7.6), (0.42, 7.3)]), ink=fine, steps=5)
        d.fill(col, Ps([(-0.66, 7.38), (-0.6, 7.28), (0.0, 7.24), (0.6, 7.28), (0.66, 7.38), (0.0, 7.36)]), ink=fine, steps=4)
        d.line(w.get("band", "art_dark"), Ps([(-0.42, 7.38), (0.0, 7.4), (0.42, 7.38)]), R(0.08), taper=False)
    elif hat == "souwester":
        col = col or "art_yellow"                               # (oilskin: a brim all round, long and down over his nape)
        d.fill(col, Ps([(0.74, 7.16), (0.7, 7.08), (0.3, 7.12), (-0.1, 7.12), (-0.48, 7.02), (-0.74, 6.76), (-0.86, 6.46),
                        (-0.72, 6.36), (-0.5, 6.44), (-0.47, 6.84), (-0.36, 7.22), (0.5, 7.26)]), ink=fine, steps=4)
        d.fill(col, Ps([(-0.46, 7.2), (-0.5, 7.5), (-0.3, 7.74), (0.0, 7.8), (0.3, 7.74), (0.5, 7.5), (0.48, 7.22), (0.0, 7.26)]), ink=fine, steps=5)
        d.line("art_ink", Ps([(-0.34, 7.66), (-0.12, 7.44), (-0.08, 7.26)]), R(0.025))
        d.line("art_ink", Ps([(0.3, 7.7), (0.22, 7.46), (0.2, 7.26)]), R(0.025))
    elif hat == "shako":
        col = col or "art_white"
        d.fill(w.get("plume", "art_red"), Ps([(0.0, 8.3), (-0.14, 8.6), (-0.04, 9.0), (0.12, 8.7), (0.16, 8.36)]), ink=fine, steps=4)
        d.shape(col, Ps([(-0.44, 7.2), (-0.5, 8.36), (0.5, 8.36), (0.44, 7.2)]), ink=fine)
        d.shape(w.get("band", "art_gold"), Ps([(-0.45, 7.22), (0.45, 7.22), (0.46, 7.38), (-0.46, 7.38)]), ink=fine * 0.7)
        d.fill("art_black", Ps([(-0.44, 7.24), (0.0, 7.15), (0.56, 7.2), (0.46, 7.28), (0.0, 7.28)]), ink=fine * 0.8, steps=4)
        d.oval(w.get("badge", "art_gold"), *P(0.0, 7.82), R(0.13), R(0.15), ink=fine * 0.6)
    elif hat in ("flying", "goggles"):
        lid = w.get("helmet", "art_brown")
        cx, cy = P(0.035, 6.86)
        size = R(0.94) * F.hw
        if hat == "flying":
            helmet(d, cx, cy, size, F.s, paint=lid, view="front", part="cap", inside=None, ink=fine, turn=-F.tilt * F.s,
                   wide=1.0, strap=w.get("strap", True))
        else:                                                   # (goggles alone: their band round to his head's edge)
            g = _HELMET["front"]
            S2 = lambda u, v: P(0.035 + 0.94 * u, 6.86 + 0.94 * F.hw * v)     # noqa: E731
            edge = _hull(list(F.head_edge) + list(F.hair_edge or []))
            _band(d, "art_black", smooth([S2(-0.5, 0.36), S2(0.0, 0.47), S2(0.5, 0.36)], False, 5), size * 0.045, fine * 0.5,
                  within=edge, reach=(True, True))
            for lu, lv, lrx, lry in g["lenses"]:
                d.oval("art_lsteel", *S2(lu, lv - 0.08), lrx * size, lry * size, turn=-F.tilt * F.s, ink=fine * 0.9)
                d.oval("art_sky", *S2(lu, lv - 0.08), lrx * size * 0.68, lry * size * 0.68, turn=-F.tilt * F.s, ink=fine * 0.45)
    else:
        raise ValueError(f"no such hat: {hat}")


# ---- his clothes ------------------------------------------------------------------------------------------------------
def _cut(d, F, paint, pts, ink, stripes=None, direction="v", bands=None):
    """A garment of his body (its points in his body's frame, a point twice a sharp corner), and its stripes."""
    up = _closed(pts)
    d.shape(paint, F.Ps(up), ink=0.0 if stripes else ink)
    if stripes:
        for lo, hi in bands or ((-0.89, -0.75), (-0.57, -0.43), (-0.25, -0.11), (0.11, 0.25), (0.43, 0.57), (0.75, 0.89)):
            piece = _clip(up, 0 if direction == "v" else 1, lo, hi)
            if len(piece) > 2:
                d.shape(stripes, F.Ps(piece), ink=0.0)
        d.shape(None, F.Ps(up), ink=ink)                          # (its edge over the stripes' ends)
    return up


JACKETS = {   # (his body's frame; a point twice is a sharp corner)
    "suit": [(-0.88, 3.46), (-0.88, 3.46), (-0.9, 3.9), (-0.82, 4.5), (-0.86, 5.0), (-0.97, 5.5), (-1.19, 5.72), (-1.16, 5.94),
             (-1.0, 6.05), (-0.76, 6.09), (-0.27, 6.14), (-0.27, 6.14), (0.0, 4.95), (0.0, 4.95), (0.27, 6.14), (0.27, 6.14),
             (0.76, 6.09), (1.0, 6.05), (1.16, 5.94), (1.19, 5.72), (0.97, 5.5), (0.86, 5.0), (0.82, 4.5), (0.9, 3.9), (0.88, 3.46),
             (0.88, 3.46), (0.14, 3.4), (0.0, 3.6), (0.0, 3.6), (-0.14, 3.4)],
    "tunic": [(-0.92, 3.18), (-0.92, 3.18), (-0.9, 3.7), (-0.82, 4.5), (-0.86, 5.0), (-0.97, 5.5), (-1.19, 5.72), (-1.16, 5.94), (-1.0, 6.05), (-0.76, 6.09),
              (-0.28, 6.14), (0.28, 6.14), (0.76, 6.09), (1.0, 6.05), (1.16, 5.94), (1.19, 5.72), (0.97, 5.5), (0.86, 5.0), (0.82, 4.5), (0.9, 3.7), (0.92, 3.18),
              (0.92, 3.18), (0.0, 3.14)],
    "flight": [(-0.9, 4.26), (-0.9, 4.26), (-0.87, 4.7), (-0.9, 5.1), (-0.97, 5.5), (-1.22, 5.72), (-1.19, 5.94), (-1.0, 6.05), (-0.78, 6.09), (-0.28, 6.14),
               (-0.28, 6.14), (0.0, 5.18), (0.0, 5.18), (0.28, 6.14), (0.28, 6.14), (0.78, 6.09), (1.0, 6.05), (1.19, 5.94), (1.22, 5.72), (0.97, 5.5),
               (0.9, 5.1), (0.87, 4.7), (0.9, 4.26), (0.9, 4.26), (0.0, 4.22)],
    "short": [(-0.86, 4.3), (-0.86, 4.3), (-0.84, 4.7), (-0.88, 5.1), (-0.97, 5.5), (-1.20, 5.72), (-1.17, 5.94), (-1.0, 6.05), (-0.77, 6.09), (-0.27, 6.14),
              (-0.27, 6.14), (0.0, 5.3), (0.0, 5.3), (0.27, 6.14), (0.27, 6.14), (0.77, 6.09), (1.0, 6.05), (1.17, 5.94), (1.20, 5.72), (0.97, 5.5), (0.88, 5.1),
              (0.84, 4.7), (0.86, 4.3), (0.86, 4.3), (0.0, 4.26)],
    "coat": [(-0.96, 2.0), (-0.96, 2.0), (-0.92, 3.0), (-0.84, 4.5), (-0.88, 5.0), (-0.97, 5.5), (-1.21, 5.72), (-1.18, 5.94), (-1.0, 6.05), (-0.78, 6.09),
             (-0.27, 6.14), (-0.27, 6.14), (0.0, 4.9), (0.0, 4.9), (0.27, 6.14), (0.27, 6.14), (0.78, 6.09), (1.0, 6.05), (1.18, 5.94), (1.21, 5.72), (0.97, 5.5),
             (0.88, 5.0), (0.84, 4.5), (0.92, 3.0), (0.96, 2.0), (0.96, 2.0), (0.0, 1.96)],
}


def _figure(d, F, build, pose, arms, w, hat, hair, hair_paint, skin, smile, mouth, eyes, moustache, cheeks, legs_shown, behind):
    ink, fine = d.ink, d.ink * 0.7
    R = F.r
    B = BUILDS[build]
    F.neck = B["neck"]                                              # (for a scarf round it)
    opts = POSE.get(pose, {}) if isinstance(pose, str) else {}
    legs = MAN_LEGS[pose] if isinstance(pose, str) else pose
    seated = opts.get("seated", False)
    shirt = w.get("shirt") or w.get("top") or "art_white"
    cov = w.get("coveralls")
    overalls = w.get("overalls")
    if cov:
        shirt = cov
    trousers = w.get("trousers") or cov or overalls
    shorts, knick = w.get("shorts"), w.get("knickers")
    if not (trousers or shorts or knick):
        trousers = "art_khaki" if build == "man" else None
        shorts = None if build == "man" else "art_khaki"
    jacket, coat = w.get("jacket"), w.get("coat")
    cut = "coat" if coat else w.get("cut", "suit") if jacket else None
    outer = coat or jacket
    if build == "kid" and cut == "suit":
        cut = "short"
    stripes = w.get("stripes")
    sdir = w.get("stripe_dir", "v" if outer else "h")
    sleeves = w.get("sleeves", "long" if (outer or cov) else "short" if build == "kid" else "long")
    if sleeves == "none":                                          # (a shirt keeps its sleeves: bare arms are a bathing suit's)
        sleeves = "long" if (outer or cov) else "short"
    if sleeves == "bare" and (outer or cov):
        sleeves = "long"
    sleeve = w.get("sleeve") or outer or shirt
    gloves = w.get("gloves")
    neck = w.get("neck") or ("high" if cut == "tunic" else "tie" if (w.get("tie") or w.get("bowtie")) else "crew" if
                             ((build == "kid" and not cov) or sleeves == "bare") else "open")
    ARMS_ALL = MAN_ARMS

    # ---- his arms: where they go (in his body's frame)
    SH = B["shoulder"]

    def place(side, spec):
        sx = 1.0 if side == "fore" else -1.0
        S = (SH[0] * sx, SH[1])
        if isinstance(spec, str):
            name = spec
            spec = ARMS_ALL[spec]
            if seated and name == "down":
                spec = ((0.1, -1.2), ((0.3 if side == "fore" else 0.55), -2.1))
        if isinstance(spec, dict) and "fore" in spec:
            spec = spec[side]
        if isinstance(spec, dict):
            tu, tv = spec["to"]
            if spec.get("frame") == "head":
                tu, tv = F.to_body(*F.hd(tu, tv))
            if spec.get("elbow"):                                  # (its elbow fixed, its hand sent to the point)
                eu, ev = spec["elbow"]
                return S, (S[0] + eu * sx, S[1] + ev), (tu, tv), spec.get("hand", True)
            E, H = _reach(S, (tu, tv), spec.get("bend", "down"), sx)
            return S, E, H, spec.get("hand", True)
        if isinstance(spec[0], (int, float)):
            E, H = _reach(S, spec, "down", sx)
            return S, E, H, True
        el, hn = spec
        return S, (S[0] + el[0] * sx, S[1] + el[1]), (S[0] + hn[0] * sx, S[1] + hn[1]), True

    placed = {"fore": place("fore", arms[0]), "aft": place("aft", arms[1])}

    def arm(side):
        S, E, H, hand_on = placed[side]
        sx = 1.0 if side == "fore" else -1.0
        chain = [S, lerp(S, E, 0.5), E, lerp(E, H, 0.45), H]
        ss = stripes if sleeve == (outer or shirt) else None        # (his sleeves striped as the garment they belong to)
        if sleeves == "long":
            G, rr = F.Ps(chain), [R(r) for r in B["sleeve"]]
            if ss and sdir == "h":                                  # (rings round it, in step with his body's stripes)
                d.limb(sleeve, G, rr, ink=0.0)
                _rings(d, ss, G, rr, R(0.38), R(0.22), R(0.16))
                d.limb(None, G, rr, ink=ink)
            else:
                d.limb(sleeve, G, rr, ink=ink)
            if ss and sdir == "v":
                for off in (-0.12, 0.12):
                    d.line(ss, F.Ps(_side_line([lerp(S, E, 0.12), E, lerp(E, H, 0.82)], off)), R(0.085), taper=False)
            cuff = w.get("knit") if cut == "flight" else w.get("trim") if cut == "tunic" else None
            if cuff:
                d.limb(cuff, F.Ps([lerp(E, H, 0.8), H]), [R(0.2), R(0.19)], ink=fine)
            else:
                cu = lerp(E, H, 0.84)
                dx, dy = H[0] - E[0], H[1] - E[1]
                ln = math.hypot(dx, dy) or 1.0
                nx, ny = -dy / ln * 0.19, dx / ln * 0.19
                d.line("art_ink", F.Ps([(cu[0] + nx, cu[1] + ny), (cu[0] - nx, cu[1] - ny)]), R(0.03), taper=False)
        else:
            d.limb(skin, F.Ps(chain), [R(r) for r in B["arm"]], ink=fine)
            if sleeves == "rolled":                                 # (pushed up above his elbow, its cuff turned back twice)
                _sleeve_shape(d, F, sleeve, _sleeve(S, E, 0.0, 0.8, 0.3, 0.27), ink, ss, sdir, S, E)
                _sleeve_shape(d, F, sleeve, _sleeve(S, E, 0.7, 0.92, 0.29, 0.28, cap=False), fine, ss, sdir, S, E)
            elif sleeves == "short":                                # (a straight hem halfway to his elbow)
                _sleeve_shape(d, F, sleeve, _sleeve(S, E, 0.0, 0.52, 0.3, 0.285), ink, ss, sdir, S, E)
        if not hand_on:
            return
        dx, dy = H[0] - E[0], H[1] - E[1]
        ln = math.hypot(dx, dy) or 1.0
        ux, uy = dx / ln, dy / ln
        hc = (H[0] + ux * 0.12, H[1] + uy * 0.12)
        g0, g1 = F.P(*E), F.P(*H)
        gl = math.hypot(g1[0] - g0[0], g1[1] - g0[1]) or 1.0
        gdir = ((g1[0] - g0[0]) / gl, (g1[1] - g0[1]) / gl)
        ang = math.degrees(math.atan2(gdir[1], gdir[0]))
        flat = isinstance(arms[0 if side == "fore" else 1], str) and arms[0 if side == "fore" else 1] == "salute"
        paint = gloves or skin
        hx_, hy_ = B["hand"]
        rx, ry = R(hx_ * (1.25 if flat else 1.0)), R(hy_ * (0.65 if flat else 1.0))
        d.oval(paint, *F.P(*hc), rx, ry, turn=ang, ink=fine)
        if not flat:
            d.oval(paint, *F.P(hc[0] - uy * 0.1 * sx, hc[1] + ux * 0.1 * sx), R(0.085), R(0.055), turn=ang + 50 * F.s, ink=fine * 0.8)
        F.hand[side] = (F.P(*hc), gdir)
        F.grips[side] = (paint, F.P(*hc), rx, ry, ang, fine)

    for side in behind:
        arm(side)

    # ---- his legs
    front = opts.get("front", False)
    leg_tops = []                                                   # (each leg's outer garment, for his trousers' top to join)

    def carried(p):
        """A hip carried by his body's lean about the point it leans on (his knee and foot stay planted), so that his
        legs' tops stay under his trousers' top, which leans with him."""
        if not F.tilt:
            return p
        pv = (3.8 - F.drop) * F.c
        du, dv = F._turn(p[0] * F.wide * F.c, p[1] * F.c - pv)
        return (du / (F.wide * F.c), (dv + pv) / F.c)

    def garment(paint, pts, radii, w_ink):
        """A leg of his trousers (shorts, knickers), its top flat and uninked under his trousers' top; in front of him
        (curled up) a plain limb, its top round."""
        G, rr = F.PLs(pts), [R(r) for r in radii]
        if front:
            d.limb(paint, G, rr, ink=w_ink)
        else:
            leg_tops.append(_flat_limb(d, paint, G, rr, w_ink))

    def boot_top(shoe, knee, ank):
        """A boot's shaft over his trouser leg (tucked): straight sides as wide as the leg down to his ankle, its top cut
        flat and a little slanted, and his trouser leg bloused over that top in a short fold."""
        K, A = F.PL(*knee), F.PL(*ank)
        ln = math.hypot(A[0] - K[0], A[1] - K[1]) or 1.0
        ux, uy = (A[0] - K[0]) / ln, (A[1] - K[1]) / ln              # (down his shin)
        nx, ny = -uy, ux
        fs = 1.0 if nx * F.s >= 0 else -1.0                         # (the side of his shin his toes are on)
        T0 = lerp(K, A, 0.3)

        def at(along, across):
            return (T0[0] + ux * along + nx * across, T0[1] + uy * along + ny * across)

        ht, ha, lift = R(0.385), R(0.36), R(0.07)

        def top_of(across):                                         # (the shaft's top edge: its front corner the higher)
            return -lift * (across * fs / ht + 1.0) / 2.0
        Z = (A[0] + ux * R(0.12), A[1] + uy * R(0.12))              # (its foot, painted over it, hides its flat bottom)
        d.shape(shoe, [at(top_of(ht), ht), (Z[0] + nx * ha, Z[1] + ny * ha), (Z[0] - nx * ha, Z[1] - ny * ha), at(top_of(-ht), -ht)], ink=ink)
        if trousers:
            hw, hb = R(0.355), R(0.43)
            fold = [at(top_of(-ht) - R(0.13), -hw), at(top_of(-ht) - R(0.02), -hb), at(top_of(-ht) + R(0.07), -R(0.4)), at(top_of(0.0) + R(0.11), 0.0),
                    at(top_of(ht) + R(0.07), R(0.4)), at(top_of(ht) - R(0.02), hb), at(top_of(ht) - R(0.13), hw)]
            ring = ccw(smooth(fold, False, 4))
            d.shape(trousers, ring, ink=0.0)
            d._put("art_ink", d._edge(ring, ink, closed=False))       # (no ink along its top: it is his trouser leg going on)

    def one_leg(pts, fore):
        hip, knee, ank = pts
        hip = carried(hip)
        side = "fore" if fore else "aft"
        heel_up = opts.get("heel") == side
        chain = [hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.3), ank]
        boots = w.get("boots")
        shoe = boots or w.get("shoes", "art_brown")
        ax, ay = ank

        def foot():
            if heel_up:
                pts_ = [(ax - 0.15, ay + 0.06), (ax + 0.15, ay + 0.06), (ax + 0.3, ay - 0.3), (ax + 0.22, ay - 0.56), (ax + 0.04, ay - 0.56),
                        (ax - 0.12, ay - 0.36), (ax - 0.17, ay - 0.1)]
            else:
                pts_ = [(ax - 0.21, ay + 0.04), (ax + 0.12, ay + 0.05), (ax + 0.36, ay - 0.08), (ax + 0.55, ay - 0.2), (ax + 0.57, ay - 0.34),
                        (ax + 0.44, ay - 0.42), (ax - 0.22, ay - 0.42), (ax - 0.27, ay - 0.2)]
            d.fill(shoe, F.PLs(pts_), ink=fine, steps=3)
            if not heel_up:
                d.line("art_ink", F.PLs([(ax - 0.22, ay - 0.35), (ax + 0.12, ay - 0.355), (ax + 0.5, ay - 0.33)]), R(0.04), taper=False)

        if boots and w.get("tucked"):
            if trousers:                                            # (his trouser leg ends inside the boot's shaft)
                garment(trousers, chain[:-1] + [lerp(knee, ank, 0.55)], B["trouser"][:-1] + [0.34], ink)
                d.line("art_ink", F.PLs([lerp(hip, knee, 0.35), knee]), R(0.035))
            else:
                d.limb(skin, F.PLs(chain), [R(r) for r in B["leg"]], ink=fine)
            boot_top(shoe, knee, ank)
            foot()
            return
        foot()
        if boots:
            d.limb(shoe, F.PLs([lerp(knee, ank, 0.62), ank]), [R(0.27), R(0.26)], ink=fine)
        if trousers:
            garment(trousers, chain[:-1] + [lerp(knee, ank, 0.94)], B["trouser"], ink)
            d.line("art_ink", F.PLs([lerp(hip, knee, 0.35), knee, lerp(knee, ank, 0.75)]), R(0.035))
        else:
            d.limb(skin, F.PLs(chain[:-1] + [lerp(knee, ank, 0.96)]), [R(r) for r in B["leg"]], ink=fine)
            if w.get("socks") and not knick:
                d.limb(w["socks"], F.PLs([lerp(knee, ank, 0.18), lerp(knee, ank, 0.6), lerp(knee, ank, 0.97)]), [R(0.22), R(0.2), R(0.17)], ink=fine)
            if shorts:
                garment(shorts, [hip, lerp(hip, knee, 0.4), lerp(hip, knee, 0.68)], [0.5, 0.47, 0.44], ink)
            if knick:
                d.limb(w.get("socks", "art_lgrey"), F.PLs([lerp(knee, ank, 0.2), lerp(knee, ank, 0.6), lerp(knee, ank, 0.97)]),
                       [R(0.22), R(0.2), R(0.17)], ink=fine)
                garment(knick, [hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.2)], [0.52, 0.5, 0.47, 0.42], ink)
                d.line("art_ink", F.PLs([lerp(hip, knee, 0.35), knee]), R(0.035))

    def all_legs():
        if not legs_shown:
            return
        if legs[1]:
            one_leg(legs[1], False)
        one_leg(legs[0], True)

    if not front:
        all_legs()

    # ---- his body: shirt, trousers' top, belt; his collar; bib, apron, vest; jacket or coat
    body = _cut(d, F, shirt, B["torso"], ink, stripes=stripes if (stripes and not outer) else None, direction=sdir,
                bands=None if sdir == "v" else ((4.62, 4.84), (5.0, 5.22), (5.38, 5.6), (5.76, 5.98)))
    if sleeves == "bare":                                          # (a bathing suit: cut deep round his arms, narrow straps)
        for sx in (-1.0, 1.0):
            rim = [(u, v) for u, v in body if u * sx >= 0.6 and v >= 5.28]
            if rim[0][1] < rim[-1][1]:
                rim.reverse()                                       # (from his shoulder's top down to his armpit)
            (au, av) = rim[-1]
            hole = smooth([(au, av), (sx * 0.72, 5.44), (sx * 0.6, 5.68), (sx * 0.53, 5.92), (sx * 0.5, 6.12)], False, 4)
            d.shape(skin, F.Ps(rim + hole[1:]), ink=0.0)
            hp = F.Ps(hole)
            d._put("art_ink", d._edge(hp, ink, closed=False)[:len(hp) - 1])
    if trousers or shorts or knick:
        tp = trousers or shorts or knick
        lo = 3.48 if seated else 3.28
        _trouser_top(d, F, tp, seated, lo, leg_tops if (leg_tops and not front) else None, ink)
        d.line("art_ink", F.Ps([(0.04, 4.48), (0.05, max(lo + 0.22, 3.6))]), R(0.035))
        if (w.get("belt") or not (overalls or cov or w.get("braces") or w.get("apron") or "belt" in w)) and not overalls:
            belt = w.get("belt") or ("art_brown" if build == "man" else tp)       # (belt=None or False: none)
            d.shape(belt, F.Ps([(-0.78, 4.8), (0.78, 4.8), (0.78, 4.6), (-0.78, 4.6)]), ink=fine)
            if w.get("belt"):
                d.shape(w.get("buckle", "art_gold"), F.Ps([(-0.1, 4.84), (0.12, 4.84), (0.12, 4.56), (-0.1, 4.56)]), ink=fine * 0.8)
    if cov:                                                        # (a coverall: one piece, its front closed down to the crotch)
        d.line("art_ink", F.Ps([(0.03, 5.6), (0.04, 4.8), (0.05, 3.7)]), R(0.035))
        for sx in (-1, 1):
            d.shape(cov, F.Ps([(sx * -0.12, 5.12), (sx * -0.56, 5.14), (sx * -0.56, 5.0), (sx * -0.12, 4.98)]), ink=fine * 0.8)
    # (his neck, his neckline and his collar)
    nk = B["neck"]
    d.shape(skin, F.PHs([(-nk, 5.98), (nk, 5.98), (nk, 6.46), (-nk, 6.46)]), ink=0.0)
    for sx in (-1, 1):
        d.line("art_ink", F.PHs([(sx * nk, 6.08), (sx * (nk - 0.01), 6.3), (sx * nk, 6.46)]), F.rh(0.04), taper=False)
    if neck == "crew":
        nl = [(-0.27, 6.13), (-0.18, 5.97), (0.0, 5.92), (0.18, 5.97), (0.27, 6.13)]
        d.shape(skin, F.Ps(nl + [(0.25, 6.3), (-0.25, 6.3)]), ink=0.0)
        d.line("art_ink", F.Ps(nl), R(0.05), taper=False)
    elif neck == "open":
        d.shape(skin, F.Ps([(-0.21, 6.16), (0.02, 5.6), (0.23, 6.16), (0.2, 6.3), (-0.2, 6.3)]), ink=0.0)
        d.line("art_ink", F.Ps([(-0.21, 6.13), (0.02, 5.62), (0.23, 6.13)]), R(0.04), taper=False)
        for sx in (-1, 1):
            d.shape(w.get("collar", shirt), F.Ps([(sx * 0.22, 6.17), (sx * 0.48, 6.1), (sx * 0.52, 5.84), (sx * 0.12, 5.86)]), ink=fine)
    elif neck == "tie":
        d.shape(skin, F.Ps([(-0.2, 6.16), (0.0, 5.98), (0.2, 6.16), (0.2, 6.3), (-0.2, 6.3)]), ink=0.0)
        for sx in (-1, 1):
            d.shape(w.get("collar", shirt), F.Ps([(sx * 0.02, 5.98), (sx * 0.24, 6.17), (sx * 0.38, 6.05), (sx * 0.24, 5.76)]), ink=fine)
        if w.get("tie"):
            d.shape(w["tie"], F.Ps([(-0.08, 6.0), (0.08, 6.0), (0.12, 5.04), (0.0, 4.9), (-0.12, 5.04)]), ink=fine)
            d.shape(w["tie"], F.Ps([(-0.1, 6.02), (0.1, 6.02), (0.07, 5.88), (-0.07, 5.88)]), ink=fine)
    if overalls:                                                   # (bib overalls: the bib, its pocket, the straps)
        d.fill(overalls, F.Ps([(-0.5, 4.5), (-0.48, 5.42), (0.48, 5.42), (0.5, 4.5)]), ink=fine, steps=2)
        d.fill(overalls, F.Ps([(-0.19, 4.94), (0.19, 4.94), (0.19, 5.18), (-0.19, 5.18)]), ink=fine * 0.7, steps=2)
        for sx in (-1, 1):
            d.limb(overalls, F.Ps([(sx * 0.4, 5.36), (sx * 0.52, 5.76), (sx * 0.6, 6.1)]), [R(0.085)] * 3, ink=fine)
            d.oval(w.get("buttons", "art_lsteel"), *F.P(sx * 0.4, 5.32), R(0.065), R(0.065), ink=fine * 0.6)
    if w.get("braces"):
        for sx in (-1, 1):
            d.limb(w["braces"], F.Ps([(sx * 0.42, 4.7), (sx * 0.5, 5.3), (sx * 0.6, 6.06)]), [R(0.06)] * 3, ink=fine * 0.8)
    if w.get("apron"):                                              # (one piece: its bib, and its skirt from his waist to his knee)
        ap = w["apron"]
        bib = w.get("apron_cut", "bib") == "bib"
        lo = 3.4 if seated else 2.2
        pts = [(-0.68, 4.64), (-0.68, 4.64), (-0.86, 3.4), (-0.88, lo), (-0.88, lo), (0.88, lo), (0.88, lo), (0.86, 3.4), (0.68, 4.64), (0.68, 4.64)]
        if bib:
            pts += [(0.44, 4.72), (0.44, 4.72), (0.45, 5.46), (0.45, 5.46), (-0.45, 5.46), (-0.45, 5.46), (-0.44, 4.72), (-0.44, 4.72)]
        else:
            pts += [(0.0, 4.68)]
        _cut(d, F, ap, pts, fine)
        if not seated:
            d.line("art_ink", F.Ps([(-0.26, 4.3), (-0.32, 3.3), (-0.36, lo + 0.3)]), R(0.03))
        if bib:
            for sx in (-1, 1):
                d.limb(ap, F.Ps([(sx * 0.4, 5.4), (sx * 0.33, 5.8), (sx * 0.26, 6.12)]), [R(0.055)] * 3, ink=fine * 0.8)
        for sx in (-1, 1):                                          # (its strings, round to the back of him)
            d.line(w.get("ties", ap), F.Ps([(sx * 0.66, 4.66), (sx * 0.8, 4.67), (sx * 0.9, 4.6)]), R(0.07), taper=False)
    if w.get("vest"):
        v = w["vest"]
        _cut(d, F, v, [(-0.77, 4.36), (-0.77, 4.36), (-0.78, 4.9), (-0.84, 5.3), (-0.9, 5.62), (-0.86, 5.86), (-0.56, 6.02), (-0.28, 6.08),
                       (-0.28, 6.08), (0.0, 5.14), (0.0, 5.14), (0.28, 6.08), (0.28, 6.08), (0.56, 6.02), (0.86, 5.86), (0.9, 5.62),
                       (0.84, 5.3), (0.78, 4.9), (0.77, 4.36), (0.77, 4.36), (0.36, 4.2), (0.0, 4.4), (0.0, 4.4), (-0.36, 4.2)], ink)
        for yy in (4.9, 4.6):
            d.oval(w.get("buttons", "art_lsteel"), *F.P(0.06, yy), R(0.055), R(0.055), ink=fine * 0.6)
    if w.get("kerchief"):
        d.fill(w["kerchief"], F.Ps([(-0.3, 6.16), (0.3, 6.16), (0.18, 5.86), (0.02, 5.52), (-0.16, 5.86)]), ink=fine, steps=3)
        d.oval(w["kerchief"], *F.P(0.02, 6.04), R(0.1), R(0.08), ink=fine * 0.8)
    if cut:                                                         # (his jacket, or his coat)
        pts = list(JACKETS[cut])
        if seated and cut in ("coat", "tunic"):
            pts = [(u, max(v, 3.42)) for u, v in pts]
        _cut(d, F, outer, pts, ink, stripes=stripes if outer else None, direction=sdir, bands=None if sdir == "v" else _BANDS_H)
        if cut in ("suit", "coat", "short"):
            vy = {"suit": 4.95, "coat": 4.9, "short": 5.3}[cut]
            bottom = {"suit": 3.6, "coat": 2.0 if not seated else 3.42, "short": 4.3}[cut]
            d.line("art_ink", F.Ps([(0.0, vy), (0.03, (vy + bottom) / 2), (0.04, bottom + 0.02)]), R(0.035), taper=False)
            lap = w.get("lapel", outer)
            big = 1.15 if cut == "coat" else 1.0
            for sx in (-1, 1):
                d.shape(lap, F.Ps([(sx * 0.27, 6.14), (sx * 0.5 * big, 6.08), (sx * 0.45 * big, 5.72), (sx * 0.34 * big, 5.66), (sx * 0.02, vy + 0.03)]), ink=fine)
            for yy in ((4.7, 4.3) if cut == "suit" else (4.5, 3.9, 3.3) if cut == "coat" else (4.9, 4.55)):
                d.oval(w.get("buttons", "art_dark"), *F.P(0.12, yy), R(0.06), R(0.06), ink=fine * 0.6)
            if cut == "suit":
                for sx in (-1, 1):
                    d.line("art_ink", F.Ps([(sx * 0.72, 4.02), (sx * 0.34, 4.04)]), R(0.035), taper=False)
                if w.get("pocket"):
                    d.shape(w["pocket"], F.Ps([(-0.66, 5.3), (-0.6, 5.44), (-0.5, 5.36), (-0.42, 5.46), (-0.38, 5.3)]), ink=fine * 0.6)
                d.line("art_ink", F.Ps([(-0.7, 5.3), (-0.36, 5.32)]), R(0.035), taper=False)
        elif cut == "tunic":
            d.line("art_ink", F.Ps([(0.0, 6.12), (0.02, 4.6), (0.03, 3.2)]), R(0.035), taper=False)
            for yy in (5.64, 5.04, 4.44, 3.84):
                d.oval(w.get("buttons", "art_gold"), *F.P(0.12, yy), R(0.07), R(0.07), ink=fine * 0.6)
        elif cut == "flight":
            d.shape(w.get("knit", outer), F.Ps([(-0.9, 4.24), (0.9, 4.24), (0.88, 4.5), (-0.88, 4.5)]), ink=fine)
            d.line("art_ink", F.Ps([(0.0, 5.18), (0.01, 4.7), (0.02, 4.26)]), R(0.035), taper=False)
            if w.get("fur"):
                for sx in (-1, 1):
                    d.fill(w["fur"], F.Ps([(sx * 0.18, 6.22), (sx * 0.62, 6.2), (sx * 0.96, 5.98), (sx * 0.86, 5.6), (sx * 0.5, 5.44),
                                           (sx * 0.16, 5.26), (sx * 0.0, 5.18), (sx * 0.12, 5.7)]), ink=fine, steps=4)
            else:
                for sx in (-1, 1):
                    d.shape(outer, F.Ps([(sx * 0.27, 6.15), (sx * 0.52, 6.1), (sx * 0.56, 5.76), (sx * 0.06, 5.24)]), ink=fine)
            for sx in (-1, 1):
                d.shape(outer, F.Ps([(sx * 0.32, 4.68), (sx * 0.72, 4.68), (sx * 0.7, 5.06), (sx * 0.34, 5.06)]), ink=fine * 0.8)
        if w.get("belt") and cut in ("coat", "tunic"):
            d.shape(w["belt"], F.Ps([(-0.86, 4.7), (0.86, 4.7), (0.86, 4.48), (-0.86, 4.48)]), ink=fine)
            d.shape(w.get("buckle", "art_gold"), F.Ps([(-0.1, 4.74), (0.12, 4.74), (0.12, 4.44), (-0.1, 4.44)]), ink=fine * 0.8)
    if neck == "high":                                              # (a tunic's stand collar, round his neck)
        d.fill(w.get("trim") or outer or shirt, F.PHs([(-0.29, 6.04), (0.29, 6.04), (0.28, 6.25), (0.0, 6.28), (-0.28, 6.25)]), ink=fine, steps=2)
    if w.get("bowtie"):
        bt = w["bowtie"]
        for sx in (-1, 1):
            d.shape(bt, F.Ps([(sx * 0.04, 6.04), (sx * 0.24, 6.14), (sx * 0.24, 5.9), (sx * 0.04, 5.98)]), ink=fine)
        d.oval(bt, *F.P(0.0, 6.01), R(0.06), R(0.06), ink=fine * 0.8)
    if w.get("scarf"):                                              # (an airman's scarf: wound round his neck; its tail last)
        scarf(d, F, w["scarf"], part="wrap", ink=fine)

    if opts.get("front"):
        all_legs()
    # ---- his head, then his arms in front of all of him
    _head(d, F, build, skin, hair, hair_paint, hat, w, smile, mouth, eyes, moustache, cheeks, fine)
    for side in ("aft", "fore"):
        if side not in behind:
            arm(side)
    if w.get("scarf"):                                              # (its tail over all of him, streaming from his nape)
        scarf(d, F, w["scarf"], part="tail", tail=w.get("scarf_tail", "aft"), ink=fine)
    return F


def _pose_frame(pose, legs_shown):
    legs = MAN_LEGS[pose] if isinstance(pose, str) else pose
    opts = POSE.get(pose, {}) if isinstance(pose, str) else {}
    hips = [legs[0][0][1]] + ([legs[1][0][1]] if legs[1] else [])
    drop = 3.8 - sum(hips) / len(hips)
    return (drop if drop > 0.15 else 0.0), opts.get("tilt", 0.0)


def man(d, x, y, h, s=1, pose="stand", arms=("down", "down"), wear=None, hat=None, hair="short", hair_paint="art_hair", skin="art_skin",
        wide=1.0, smile=True, mouth=None, eyes="open", moustache=None, cheeks=True, legs_shown=True, behind=(), tilt=None):
    """A man of the 1940s standing `h` tall (to the top of his head) with his feet at (x, y), facing s, fully dressed
    in `wear` (see this file's head for its keys, his poses, hats, hair and face), in lady()'s own hand. `tilt` leans
    his body toward the side he faces (degrees; his pose gives one), `behind` names arms painted behind him. Returns
    his frame: .hand[fore|aft] = (point, direction), .grip(side), .mouth, .face, .top, and P, PL, PH."""
    drop, lean = _pose_frame(pose, legs_shown)
    F = Body(d, x, y, h, s, wide, heads=HEADS, c=1.0, drop=drop, tilt=lean if tilt is None else tilt, hw=1.0 + 0.5 * (wide - 1.0))
    return _figure(d, F, "man", pose, arms, dict(wear or {}), hat, hair, hair_paint, skin, smile, mouth, eyes, moustache, cheeks,
                   legs_shown, tuple(behind))


def kid(d, x, y, h, s=1, pose="stand", arms=("down", "down"), wear=None, hat=None, hair="short", hair_paint="art_hair", skin="art_skin",
        wide=1.0, smile=True, mouth=None, eyes="open", cheeks=True, legs_shown=True, behind=(), tilt=None):
    """A boy of about ten, `h` tall, his feet at (x, y), facing s: man()'s hand and poses, a bigger head for his height,
    a boy's clothes of the 1940s (a shirt, short trousers or knickers, socks, a cap). By default a short-sleeved shirt,
    short trousers, knee socks and shoes. Returns his frame as man() does."""
    w = {"shirt": "art_red", "shorts": "art_blue", "socks": "art_white", "shoes": "art_brown", "sleeves": "short"}
    if wear and any(k in wear for k in ("trousers", "knickers", "overalls", "coveralls")):
        w.pop("shorts")
    if wear and any(k in wear for k in ("jacket", "coat", "coveralls")):
        w["sleeves"] = "long"
    w.update(wear or {})
    drop, lean = _pose_frame(pose, legs_shown)
    heads = NECK * KID + 1.33
    F = Body(d, x, y, h, s, wide, heads=heads, c=KID, drop=drop, tilt=lean if tilt is None else tilt, hw=1.0 + 0.5 * (wide - 1.0))
    return _figure(d, F, "kid", pose, arms, w, hat, hair, hair_paint, skin, smile, mouth, eyes, None, cheeks, legs_shown, tuple(behind))
