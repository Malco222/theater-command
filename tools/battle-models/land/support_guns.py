"""The support teams' weapons (Brian, 25 Nov 2026: "There should be machine gun teams that lay down suppressing fire,
such as the German MG 42 and the US machine gun"): each army's machine gun on its tripod or wheeled mount, and its
mortar on baseplate and bipod. Made on the vehicles' kit (Works), so they come out in the same file shape as a tank's
parts: `parts`, `sockets` (where the crew's hands go), `controls` (what the game turns), near and far meshes.

    python export_support.py <out folder> [name,name,...|all] [json glb fbx]

Every weapon faces +x and stands on the ground at z = 0. Its root part is the mount (the tripod, the wheeled carriage,
the baseplate and bipod), with its origin on the ground under the pintle (a machine gun) or under the tube's foot (a
mortar). What the game turns is a second part: a machine gun's `gun` (its cradle, the gun, its sight and the fed end
of its belt: swung left and right and laid up and down about the pintle, and what the gunner carries on his shoulder
when the team moves), a mortar's `tube` (carried the same way). No national mark on anything.

Detail (Brian, 25 Nov 2026, on the first sheet: "make them more detailed with bullets feeding into the machine gun and
more realistic angles and shapes"): every cartridge is drawn on the fine mesh, a brass case with its shoulder and a
copper bullet (CARTRIDGES: each gun's own round), in its steel link or its webbing (STRIPS), on a belt that hangs from
the feed down into an open box standing end-on to the gun, the top fold of the belt lying in the box; or on a rigid
strip pushed in from the left. The fed end of a belt is part of the `gun` (so its rounds always go in under the feed
cover however she is swung and laid); the box and its fold stand on the ground with the mount. Slots, holes, bores
and flutes are dark marks a millimetre proud of the face they are on (_mark). So a machine gun is three to five
thousand triangles near (a third of them her cartridges), a mortar about 2,300; far off (the plain mesh, no rounds, no
marks: one band of the belt's colour) six hundred to eleven hundred. The MG 42 is drawn to the same measures as the
section gunner's own (the soldiers' file soldier_GER_lmg): x here = their weapon frame's x less 0.572.

The sockets every machine gun has: `muzzle` (the tracer and the flash), `feed` (where the number two's hands guide the
belt or the strip in, and which side that is), `grip.gunner.l` and `grip.gunner.r` (the gunner's hands), `seat` (on the
ground: where the gunner's own root goes, and so where the rules' key man stands; `controls.gunner.posture` says
whether he lies or sits there), `seat.loader` (the number two) and `box` (the ammunition on the ground). A mortar has
`muzzle` (the bomb goes in here, and the flash), `sight` (the gunner's eye comes to it), `grip.elevate` and
`grip.traverse` (his hands on the two cranks), `seat` (the gunner, kneeling on the left), `seat.loader` (the number
two, kneeling on the right by the muzzle) and `rounds` (the bombs laid ready).
"""
import math

from mathutils import Vector

from motorpool import X, Y, Z
from works import Works

# Paints (the weapons' own; a mount's paint is its army's, given by each builder).
STEEL = "3C3E42"          # blued or parkerised gun steel (above the art director's floor for small dark parts)
WOOD = "5E3C26"           # grips: oiled walnut, no lighter than a rifle's wood (the soldiers' 6E4329)
BAKELITE = "2A1D17"       # the MG 34's and MG 42's butt and grip (the soldiers' gunner's own paint): darker than the steel
LEATHER = "5C4430"        # the Lafette's two pads
CLOTH = "5E5946"          # a webbing belt (Browning, Vickers, Maxim): under sunlit ground's lightness
LINK = "3A3C40"           # a steel link belt: no lighter than the gun's steel
BRASS = "9C7A3C"          # a cartridge's case (the soldiers' brass), a filler cap
BULLET = "6A4630"         # the bullet in it: a copper-washed jacket (the soldiers' own)
ROUNDS = "6E5E38"         # a belt far off: brass and links as one colour
DARK = "17181A"           # a slot or a hole through a jacket, a muzzle's bore: a dark mark
EARTH = "5F5443"          # what feet and spades stand in
RUBBER = "2E2C2A"         # the condenser's hose
GLASS = "26282B"          # a sight's lens

# A cartridge: overall length, the case's, where its shoulder begins (from the base), the base's radius, the neck's.
CARTRIDGES = {
    "7.92": (0.0805, 0.0570, 0.046, 0.0060, 0.0045),     # 7.92 x 57 (MG 34, MG 42)
    ".30": (0.0848, 0.0634, 0.049, 0.0060, 0.0043),      # .30-06 (the Brownings)
    ".303": (0.0775, 0.0560, 0.045, 0.0058, 0.0043),     # .303 (the Vickers)
    "7.62R": (0.0770, 0.0537, 0.040, 0.0062, 0.0043),    # 7.62 x 54 R (the Maxim)
    "8L": (0.0750, 0.0505, 0.036, 0.0068, 0.0045),       # 8 mm Lebel (the Hotchkiss)
    "7.7": (0.0800, 0.0580, 0.046, 0.0060, 0.0044),      # 7.7 x 58 (the Type 92)
    "8B": (0.0805, 0.0590, 0.047, 0.0062, 0.0045),       # 8 x 59 (the Breda)
}
# What holds a belt's cartridges: its paint, how wide it is along the cartridge, how far its back edge is from the base.
STRIPS = {"link": ("link", 0.015, 0.022), "cloth": ("cloth", 0.032, 0.014)}


def _dim(hx, f):
    return "".join(f"{max(0, min(255, int(round(int(hx[i:i + 2], 16) * f)))):02X}" for i in (0, 2, 4))


def _weapon(name, title, fine, mount, box=None):
    v = Works(name, title, "prop", "_", fine)
    for k, hx in (("steel", STEEL), ("wood", WOOD), ("bakelite", BAKELITE), ("leather", LEATHER), ("cloth", CLOTH), ("link", LINK),
                  ("brass", BRASS), ("bullet", BULLET), ("rounds", ROUNDS), ("dark", DARK), ("earth", EARTH), ("rubber", RUBBER),
                  ("glass", GLASS), ("mount", mount), ("box", box or mount), ("flute", _dim(mount, 0.70))):
        v.m.paints[k] = hx
        v.m.paint_at[k] = list(v.m.paints).index(k)
    return v


def _cyl(v, a, b, r, paint, sides=10, r2=None):
    v.rod(a, b, r, paint, sides, r2=r2)


def _mark(v, at, out, along, long, wide, paint="dark", lift=0.001):
    """A dark mark on a face (a cooling slot, a hole, a bore): a flat six-cornered patch a hair proud of it, looking
    along `out`, `long` along `along` and `wide` across. On the fine mesh only."""
    if not v.fine:
        return
    at, out, along = Vector(at), Vector(out).normalized(), Vector(along)
    along = (along - out * along.dot(out)).normalized()
    side = out.cross(along)
    c = at + out * lift
    a, s = along * (long / 2.0), side * (wide / 2.0)
    k = max(0.0, 1.0 - 0.5 * wide / max(long, 1e-9))
    v.m.add([c - a, c - a * k - s, c + a * k - s, c + a, c + a * k + s, c - a * k + s], [(0, 1, 2, 3, 4, 5)], paint, False, lambda mid, o=out: o)


def _ring_marks(v, x0, x1, count, r, z, angles, long, wide, paint="dark", stagger=True):
    """Rows of marks along a round jacket whose axis runs along x at height z: one row at each angle (degrees round the
    axis, 0 the gunner's left, 90 the top)."""
    for j, deg in enumerate(angles):
        a = math.radians(deg)
        out = Vector((0.0, math.cos(a), math.sin(a)))
        step = (x1 - x0) / max(count - 1, 1)
        for k in range(count):
            x = x0 + step * k + (step * 0.5 if (stagger and j % 2) else 0.0)
            if x <= x1 + 1e-9:
                _mark(v, Vector((x, 0.0, z)) + out * r, out, X, long, wide, paint)


def _bore(v, at, way, r):
    _mark(v, at, way, Z if abs(Vector(way).normalized().z) < 0.9 else X, 2.0 * r, 2.0 * r)


def _round(v, at, cal, way=X, empty=False, base=True):
    """One cartridge, its base at `at`, pointing along `way`: a brass case with its shoulder, the bullet in it
    (`empty`: a fired case, no bullet)."""
    L, case, sh, rb, rn = CARTRIDGES[cal]
    at, way = Vector(at), Vector(way).normalized()
    v.lathe(at, way, [(case, rn), (sh, rb), (0.0, rb)] + ([(0.0, 0.0)] if base else []), "brass", 5)
    if not empty:
        v.lathe(at + way * case, way, [(L - case, 0.0), ((L - case) * 0.5, rn * 0.74), (0.0, rn)], "bullet", 5)


def _smooth(pts, cuts=2):
    pts = [Vector(p) for p in pts]
    for _ in range(cuts):
        out = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            out += [a * 0.75 + b * 0.25, a * 0.25 + b * 0.75]
        out.append(pts[-1])
        pts = out
    return pts


def _along(pts, pitch, first):
    """The places every `pitch` along a row of points, the first `first` in."""
    out, want, done = [], first, 0.0
    for a, b in zip(pts, pts[1:]):
        L = (b - a).length
        if L < 1e-9:
            continue
        while want <= done + L + 1e-9:
            out.append(a + (b - a) * ((want - done) / L))
            want += pitch
        done += L
    return out


def _belt(v, pts, cal, pitch, strip, x_mid, empty=False):
    """A belt of cartridges hanging through these (y, z) places, the cartridges lying along the gun about x_mid, their
    bullets forward: every round drawn on the fine mesh (brass case, darker bullet) in its links or its webbing, the
    first of them inside the feed. Far off, one band of the belt's colour. `empty`: the fired end of a belt that
    leaves the gun at the other side: the links or the webbing alone."""
    L, case, sh, rb, rn = CARTRIDGES[cal]
    paint, wide, back = STRIPS[strip]
    x0 = x_mid - L / 2.0
    row = _along(_smooth([Vector((0.0, p[0], p[1])) for p in pts]), pitch if v.fine else pitch * 3.0, pitch * 0.5)
    if len(row) < 2:
        return
    rings = []
    links = strip == "link" and v.fine
    for i, p in enumerate(row):
        way = (row[min(i + 1, len(row) - 1)] - row[max(i - 1, 0)]).normalized()
        nrm = X.cross(way)
        if v.fine and not empty:
            _round(v, p + X * x0, cal)
        if links and not empty:                       # each round's own link clipped round its case; a thin tie to the next
            v.lathe(p + X * (x0 + back), X, [(wide, rb + 0.0014), (0.0, rb + 0.0014)], paint, 5)
            c, half, th = p + X * (x0 + back + wide / 2.0), X * (wide * 0.3), nrm * 0.002
        elif v.fine or empty:
            c, half = p + X * (x0 + back + wide / 2.0), X * (wide / 2.0)
            th = nrm * ((rb + 0.0012) if not empty else (rb * 0.8 if strip == "link" else 0.002))
        else:
            c, half, th = p + X * (x0 + L * 0.45), X * (L * 0.45), nrm * rb
        rings.append([c - half - th, c + half - th, c + half + th, c - half + th])
    v.m.skin(rings, paint if (v.fine or empty) else "rounds", (True, True), False)


def _belt_box(v, at, size, cal, pitch, strip, lid="end", far=1, paint="box", wall=0.004, fill=0.80):
    """An open belt box on the ground, END-ON to the gun (its long side along y) so the belt runs straight out of it
    and up to the feed: `at` its middle (x, y), size (long, wide, high). The top fold of its belt lies in it, every
    round drawn. `far`: +1 or -1, the side of the gun it stands on (its far end is away from her). `lid`: "end", hinged
    at the far end and swung right up (the German box); "front", hinged on the long side toward the muzzle and standing
    (the Vickers's and the Maxim's); "off", taken off and laid on the ground before the box (the American M1)."""
    x, y = at[0], at[1]
    L, W, H = size
    y0, y1 = y - L / 2.0, y + L / 2.0
    cl, case, sh, rb, rn = CARTRIDGES[cal]
    zr = H * fill
    if v.fine:
        v.slab((x - W / 2, y0, 0.0), (x + W / 2, y1, wall), paint)
        v.slab((x - W / 2, y0, 0.0), (x - W / 2 + wall, y1, H), paint, bevel=0.0015)
        v.slab((x + W / 2 - wall, y0, 0.0), (x + W / 2, y1, H), paint, bevel=0.0015)
        v.slab((x - W / 2, y0, 0.0), (x + W / 2, y0 + wall, H), paint, bevel=0.0015)
        v.slab((x - W / 2, y1 - wall, 0.0), (x + W / 2, y1, H), paint, bevel=0.0015)
        v.slab((x - W / 2 + wall, y0 + wall, wall), (x + W / 2 - wall, y1 - wall, zr - rb * 0.2), STRIPS[strip][0])   # the folds under the top one
        n = int((L - 2 * wall - 2 * rb) / pitch)
        for k in range(n):
            _round(v, (x - cl / 2.0, y0 + wall + rb + 0.002 + pitch * k, zr), cal, base=False)
        back, wide = STRIPS[strip][2], STRIPS[strip][1]                                   # its links, or its webbing, across them
        v.slab((x - cl / 2.0 + back, y0 + wall + 0.001, zr - rb), (x - cl / 2.0 + back + wide, y0 + wall + 0.003 + pitch * n, zr + rb + 0.0014), STRIPS[strip][0])
    else:
        v.box((W, L, H), (x, y, H / 2.0), paint)
        v.slab((x - W / 2 + wall, y0 + wall, H - 0.006), (x + W / 2 - wall, y1 - wall, H + 0.001), "rounds")
    yf = y1 if far > 0 else y0
    if lid == "end":                                  # swung up at the far end, a little past upright
        up = Vector((0.0, far * math.sin(math.radians(14.0)), math.cos(math.radians(14.0)))) * L
        a, b = Vector((x - W / 2, yf, H)), Vector((x + W / 2, yf, H))
        cs = [a, b, b + up, a + up]
        v.plate(cs if far < 0 else cs[::-1], wall + 0.002, paint, bevel=0.002)
        if v.fine:                                    # its carrying handle, folded flat on what was the top
            mid = (a + b) / 2.0 + up * 0.5 + Vector((0.0, far, 0.0)) * 0.004
            v.tube([mid - up.normalized() * 0.055, mid - up.normalized() * 0.045 + Vector((0.0, far * 0.012, 0.0)),
                    mid + up.normalized() * 0.045 + Vector((0.0, far * 0.012, 0.0)), mid + up.normalized() * 0.055], 0.004, "steel", 4)
    elif lid == "front":                              # standing on the long side toward the muzzle, leaning forward
        up = Vector((math.sin(math.radians(18.0)), 0.0, math.cos(math.radians(18.0)))) * (W * 0.98)
        a, b = Vector((x + W / 2, y0, H)), Vector((x + W / 2, y1, H))
        v.plate([a, b, b + up, a + up], wall + 0.002, paint, bevel=0.002)
    elif lid == "off":                                # laid on the ground before the box
        v.slab((x + W / 2 + 0.03, y0 + 0.01, 0.0), (x + W / 2 + 0.03 + W, y1 + 0.01, 0.016), paint, bevel=0.003)
        if v.fine:
            lx = x + W / 2 + 0.03 + W / 2
            v.tube([(lx, y - 0.05, 0.016), (lx, y - 0.04, 0.034), (lx, y + 0.04, 0.034), (lx, y + 0.05, 0.016)], 0.004, "steel", 4)


def _foot(v, at, kind, paint="mount"):
    """What a leg stands on: a round shoe, a spade plate dug in behind it, or a claw; the earth it stands in on it."""
    x, y, _ = at
    if kind == "shoe":
        v.lathe((x, y, 0.008), Z, [(0.032, 0.0), (0.032, 0.034), (0.0, 0.04), (0.0, 0.0)], paint, v.n(10, 6))
        v.lathe((x, y, 0.0), Z, [(0.012, 0.0), (0.012, 0.045), (0.0, 0.052), (0.0, 0.0)], "earth", v.n(10, 6))
    elif kind == "spade":
        v.slab((x - 0.008, y - 0.04, 0.02), (x + 0.008, y + 0.04, 0.065), paint)
        v.slab((x - 0.012, y - 0.044, -0.01), (x + 0.012, y + 0.044, 0.024), "earth")
        v.slab((x - 0.05, y - 0.035, 0.004), (x + 0.03, y + 0.035, 0.016), paint)
        v.slab((x - 0.058, y - 0.041, 0.0), (x + 0.038, y + 0.041, 0.007), "earth")
    elif kind == "claw":
        v.slab((x - 0.03, y - 0.03, 0.006), (x + 0.03, y + 0.03, 0.02), paint)
        v.slab((x - 0.036, y - 0.036, 0.0), (x + 0.036, y + 0.036, 0.009), "earth")


def _ammo_box(v, at, size, paint="box", lid=None, handle=True):
    """A shut box on the ground (strips, bombs), its long side along x."""
    x, y = at[0], at[1]
    L, W, H = size
    v.box((L, W, H), (x, y, H / 2.0), paint, bevel=0.005)
    if v.fine:
        v.slab((x - L / 2 - 0.001, y - W / 2 - 0.001, H * 0.78), (x + L / 2 + 0.001, y + W / 2 + 0.001, H * 0.80), "flute")     # the lid's seam
        if handle:
            v.tube([(x - 0.05, y, H), (x - 0.04, y, H + 0.025), (x + 0.04, y, H + 0.025), (x + 0.05, y, H)], 0.005, "steel", 4)


def _spade_grips(v, x, z, half=0.052, high=0.115, reach=0.05):
    """A Maxim's or a Vickers's twin handles behind the receiver: two wooden grips upright, between two steel bars,
    the thumb trigger between them."""
    for zz in (z + high / 2, z - high / 2):
        v.rod((x, -half - 0.008, zz), (x, half + 0.008, zz), 0.008, "steel", 6)
        v.rod((x + reach, 0.0, zz), (x, 0.0, zz), 0.009, "steel", 6)
    for s in (1, -1):
        v.rod((x, s * half, z - high / 2 + 0.012), (x, s * half, z + high / 2 - 0.012), 0.017, "wood", 8)
    if v.fine:
        v.slab((x - 0.004, -0.018, z - 0.02), (x + 0.012, 0.018, z + 0.02), "steel")
        v.rod((x + reach, 0.0, z - high / 2), (x + reach, 0.0, z + high / 2), 0.007, "steel", 5)


def _crank(v, at, side, r=0.022, arm=0.11):
    """A Maxim's or a Vickers's crank handle on the receiver's right: a boss, the arm sloping up and back, its knob."""
    x, y, z = at
    v.lathe((x, y, z), (0, side, 0), [(0.012, 0.0), (0.012, r), (0.0, r), (0.0, 0.0)], "steel", v.n(8, 5))
    tip = Vector((x - arm * 0.9, y + side * 0.016, z + arm * 0.42))
    v.rod((x, y + side * 0.012, z), tip, 0.007, "steel", 5)
    v.ball(tip, 0.013, "steel", 6, 4)


def _handwheel(v, at, axis, r=0.04, paint="steel"):
    v.lathe(at, axis, [(0.012, 0.0), (0.012, r), (0.0, r), (0.0, 0.0)], paint, v.n(12, 6))
    if v.fine:
        ax = Vector(axis).normalized()
        v.lathe(Vector(at) + ax * 0.012, ax, [(0.01, 0.0), (0.01, r * 0.3), (0.0, r * 0.3), (0.0, 0.0)], paint, 6)


def _controls(v, posture, feed_side, left=25.0, right=25.0, low=-8.0, high=12.0, burst="long bursts of 6 to 10, a pause, again", extra=None):
    c = {
        "traverse": {"part": "cradle", "what": "the game swings the gun left and right about the pintle (+ to the right)", "limits": [-left, right]},
        "elevate": {"part": "gun", "what": "the game lays the muzzle up and down (+ up)", "limits": [low, high]},
        "recoil": {"part": "gun", "what": "a shake of 3 to 5 mm back along the barrel on each round of a burst (the game's own; no clip)", "throw": 0.004},
        "gunner": {"posture": posture, "what": "how the gunner serves her: prone (lying behind, his elbows down) or sit (sitting on the ground behind, his knees up either side)"},
        "feed": {"side": feed_side, "what": "the side the belt or the strip comes in from, and so where the number two is (socket seat.loader). The belt's fed end is part of the gun and turns with her; the box and the fold of belt in it stand on the ground"},
        "carry": {"what": "when the team moves: the game shows the `gun` part alone on the gunner's shoulder (cradle and gun together), and puts the mount away until the number two has a clip that carries it", "fire": burst},
    }
    c.update(extra or {})
    v.m.info.setdefault("file", {})["controls"] = c


# ---------------------------------------------------------------------------------------------------------------------
# Germany: the MG 42 (or, before the summer of 1942, the MG 34) on the Lafette, set low.
# ---------------------------------------------------------------------------------------------------------------------
def lafette(fine=True, gun="mg42"):
    """The MG 42 on the Lafette 42, low, as a heavy machine gun (sMG): the gun in a sprung cradle that tips about its
    front, an optical sight low on the cradle's left by the receiver's back, the trigger handle under its back end, a
    front leg of two tubes with the two leather pads it is carried by on a man's back, and two rear legs splayed
    either side of the gunner, who lies behind with the butt at his shoulder. The belt comes up out of an open steel
    box at the left, every round on it drawn, over the feed tray's lip and in under the feed cover; the emptied links
    hang out at the right. The gun is the section gunner's (the soldiers' file soldier_GER_lmg) to the same
    measures: a squared jacket of one depth with five slots on the left and the barrel's door on the right, a flared
    booster, the receiver's back sloping to the butt's neck, a fishtail butt of bakelite. With gun="mg34", the MG 34
    in the same mount (the Lafette 34): a round jacket with rows of holes, a round receiver and a slimmer butt."""
    is42 = gun == "mg42"
    v = _weapon("HmgMG42" if is42 else "HmgMG34", ("MG 42" if is42 else "MG 34") + " on the Lafette, set low", fine, "484A46", "50524A")
    pz, gz = 0.30, 0.42                       # the pivot's height (the cradle's front), the barrel's
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the cradle's front pivot; the game places it")
    # The lower frame: the head under the pivot, a beam back to the traverse arc, the arc itself.
    v.lathe((0.0, 0.0, 0.22), Z, [(0.07, 0.0), (0.07, 0.034), (0.0, 0.034), (0.0, 0.0)], "mount", v.n(10, 6))
    v.slab((-0.62, -0.022, 0.185), (0.0, 0.022, 0.225), "mount", bevel=0.004)
    v.slab((-0.655, -0.14, 0.175), (-0.595, 0.14, 0.205), "mount", bevel=0.004)
    if fine:                                  # the arc's scale, and the beam's two lightening slots
        v.slab((-0.652, -0.13, 0.205), (-0.64, 0.13, 0.209), "steel")
        for x in (-0.44, -0.20):
            _mark(v, (x, 0.022, 0.205), Y, X, 0.13, 0.016)
            _mark(v, (x, -0.022, 0.205), -Y, X, 0.13, 0.016)
    # The front leg: two tubes to a clawed foot, the two pads lying on them.
    top, foot = Vector((0.03, 0.0, 0.225)), Vector((0.47, 0.0, 0.018))
    for s in (1, -1):
        v.rod(top + Y * (s * 0.042), foot + Y * (s * 0.042), 0.013, "mount", 6)
        v.rod(top + Y * (s * 0.042), top + (foot - top) * 0.2 + Y * (s * 0.042), 0.018, "mount", 6)      # the sleeve the tube slides in
    v.rod(foot - Y * 0.075, foot + Y * 0.075, 0.014, "mount", 6)
    v.slab((foot.x - 0.01, -0.07, 0.012), (foot.x + 0.012, 0.07, 0.03), "mount")
    v.slab((foot.x - 0.016, -0.076, 0.0), (foot.x + 0.018, 0.076, 0.014), "earth")
    along = (foot - top).normalized()
    up = Y.cross(along).normalized()
    if up.z < 0:
        up = -up
    for t in (0.34, 0.72):
        c = top + (foot - top) * t + up * (0.013 + 0.036)
        v.plate([c - along * 0.075 - Y * 0.075, c + along * 0.075 - Y * 0.075, c + along * 0.075 + Y * 0.075, c - along * 0.075 + Y * 0.075][::-1],
                0.036, "leather", bevel=0.008)
    # The rear legs, splayed, on spade feet: a sleeve at the frame, the clamp half way down.
    for s in (1, -1):
        a, b = Vector((-0.60, s * 0.11, 0.19)), Vector((-0.73, s * 0.41, 0.03))
        v.rod(a, b, 0.015, "mount", 6)
        v.rod(a, a + (b - a) * 0.22, 0.02, "mount", 6)
        if fine:
            m = a + (b - a) * 0.55
            v.rod(m - (b - a).normalized() * 0.02, m + (b - a).normalized() * 0.02, 0.021, "steel", 6)
        _foot(v, (-0.73, s * 0.41, 0.0), "spade")
    # The cradle's carrier: what swings left and right on the arc (the search-fire gear and its handwheel).
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-25.0, 25.0], kind="turn",
           note="swings about the cradle's front pivot along the arc: + the muzzle to the right")
    v.lathe((0.0, 0.0, pz - 0.02), Z, [(0.05, 0.0), (0.05, 0.026), (0.0, 0.026), (0.0, 0.0)], "mount", v.n(10, 6))
    v.slab((-0.66, -0.035, 0.205), (-0.58, 0.035, 0.30), "mount", bevel=0.004)
    v.rod((-0.62, 0.035, 0.25), (-0.62, 0.062, 0.25), 0.008, "steel", 6)
    _handwheel(v, (-0.62, 0.062, 0.25), Y, 0.042)
    if fine:                                  # the search-fire gear's lever at the right, the clamp on the arc
        v.rod((-0.62, -0.035, 0.27), (-0.60, -0.075, 0.285), 0.006, "steel", 5)
        v.ball((-0.60, -0.075, 0.285), 0.011, "steel", 6, 4)
        v.slab((-0.668, -0.02, 0.165), (-0.582, 0.02, 0.205), "steel", bevel=0.003)
    v.socket("grip.gunner.l", (-0.62, 0.085, 0.25), Y, part="cradle", note="the gunner's left hand on the elevating wheel (or steadying the butt)")
    # The gun in its cradle: tips about the cradle's front.
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.03), axis=(0, -1, 0), limits=[-8.0, 12.0], kind="turn",
           note="the cradle and the gun tip about the cradle's front: + the muzzle up; and what is carried on a shoulder")
    for s in (1, -1):                         # the cradle: two rails, the buffer springs' housings on them
        v.rod((-0.68, s * 0.034, 0.345), (0.02, s * 0.034, 0.345), 0.011, "mount", 6)
        v.rod((-0.36, s * 0.034, 0.345), (-0.10, s * 0.034, 0.345), 0.021, "mount", 8)
    v.slab((-0.03, -0.045, 0.325), (0.03, 0.045, gz - 0.02), "mount", bevel=0.004)          # the front clamp
    v.slab((-0.70, -0.04, 0.325), (-0.63, 0.04, gz - 0.035), "mount", bevel=0.004)          # the rear clamp and trigger gear
    v.rod((-0.71, 0.0, 0.335), (-0.775, 0.0, 0.255), 0.016, "bakelite", 8)                   # the trigger handle
    v.socket("grip.gunner.r", (-0.745, 0.0, 0.29), -X, part="gun", note="the gunner's right hand on the cradle's trigger handle")
    # The sight, low on the cradle's left beside the receiver's back: a bracket, the body, the eyepiece back to his eye.
    sx, sy, sz = -0.635, 0.098, gz + 0.062
    v.rod((sx, 0.04, 0.35), (sx, sy, sz - 0.02), 0.008, "mount", 5)
    v.box((0.055, 0.03, 0.04), (sx, sy, sz), "mount", bevel=0.004)
    v.rod((sx - 0.0275, sy, sz), (sx - 0.075, sy, sz), 0.011, "mount", 8)
    v.rod((sx + 0.0275, sy, sz + 0.006), (sx + 0.06, sy, sz + 0.006), 0.012, "mount", 8)
    if fine:
        _mark(v, (sx - 0.075, sy, sz), -X, Z, 0.016, 0.016, "glass")
        _mark(v, (sx + 0.06, sy, sz + 0.006), X, Z, 0.018, 0.018, "glass")
        v.rod((sx, sy + 0.015, sz), (sx, sy + 0.028, sz), 0.009, "steel", 6)                 # its range drum
    v.socket("sight", (sx - 0.08, sy, sz), -X, part="gun", note="the optical sight's eyepiece: the gunner's eye for long fire")
    # The gun. (x here = the soldiers' weapon frame's x less 0.572: her muzzle at 0.30, her heel at -0.92.)
    if is42:
        zf = gz + 0.029                                                                      # the belt's rounds as they go in
        v.box((0.435, 0.050, 0.064), (-0.0545, 0.0, gz + 0.005), "steel", bevel=0.009)       # the squared jacket: one depth to the booster
        for k in range(5):                                                                   # five slots on the left
            _mark(v, (-0.232 + 0.054 * k, 0.025, gz + 0.005), Y, X, 0.042, 0.017)
        _mark(v, (-0.110, -0.025, gz + 0.005), -Y, X, 0.21, 0.023)                           # the barrel's door on the right
        if fine:
            v.rod((-0.205, -0.0235, gz + 0.004), (-0.015, -0.0235, gz + 0.004), 0.0085, "steel", 6, caps=False)   # the barrel seen in it
            for k in range(4):                                                               # the slots on top
                _mark(v, (-0.20 + 0.062 * k, 0.0, gz + 0.037), Z, X, 0.036, 0.012)
        v.lathe((0.163, 0.0, gz), X, [(0.137, 0.0), (0.137, 0.0195), (0.107, 0.015), (0.040, 0.0152), (0.036, 0.020), (0.0, 0.020), (0.0, 0.0)],
                "steel", v.n(12, 6))                                                         # the booster: a collar, the cone, its flared mouth
        _bore(v, (0.30, 0.0, gz), X, 0.0135)
        rec = [(-0.272, -0.028), (-0.272, 0.052), (-0.367, 0.052), (-0.367, 0.036), (-0.452, 0.036), (-0.452, 0.022), (-0.548, 0.022),
               (-0.548, 0.036), (-0.562, 0.036), (-0.562, 0.052), (-0.622, 0.052), (-0.682, 0.030), (-0.682, -0.028)]
        v.extrude([(x, gz + z) for x, z in rec], -0.0245, 0.0245, "steel", bevel=0.004)      # the receiver, its back sloping to the butt's neck
        v.extrude([(x, gz + z) for x, z in ((-0.566, 0.036), (-0.556, 0.062), (-0.40, 0.062), (-0.364, 0.051), (-0.364, 0.036))],
                  -0.027, 0.027, "steel", bevel=0.004)                                       # the feed cover over the belt
        butt = [(-0.682, 0.030), (-0.910, 0.036), (-0.921, -0.030), (-0.922, -0.080), (-0.906, -0.106), (-0.884, -0.084), (-0.812, -0.058), (-0.682, -0.028)]
        v.extrude([(x, gz + z) for x, z in butt], -0.02, 0.02, "bakelite", bevel=0.007)      # the fishtail butt
        v.rod((-0.700, 0.0, gz + 0.001), (-0.676, 0.0, gz + 0.001), 0.031, "steel", 8)       # the buffer's ring at its neck
        if fine:
            v.slab((-0.345, -0.012, gz + 0.052), (-0.285, 0.012, gz + 0.060), "steel", bevel=0.002)      # the rear sight's bed and its leaf
            v.slab((-0.292, -0.011, gz + 0.060), (-0.286, 0.011, gz + 0.078), "steel")
            v.slab((0.118, -0.004, gz + 0.037), (0.134, 0.004, gz + 0.066), "steel")                     # the front sight, up
            v.slab((0.105, -0.009, gz + 0.037), (0.145, 0.009, gz + 0.043), "steel")
            v.rod((-0.36, -0.0245, gz - 0.006), (-0.36, -0.058, gz - 0.006), 0.006, "steel", 5)          # the cocking handle at the right
            v.rod((-0.36, -0.058, gz - 0.006), (-0.36, -0.078, gz - 0.006), 0.011, "steel", 6)
            _mark(v, (-0.44, -0.0245, gz - 0.006), -Y, X, 0.19, 0.008)                                   # and its slot
            v.slab((0.10, -0.014, gz - 0.04), (0.135, 0.014, gz - 0.027), "steel", bevel=0.002)          # the bipod's lug under the jacket's front
    else:
        zf = gz + 0.033
        v.rod((-0.40, 0.0, gz), (0.17, 0.0, gz), 0.0185, "steel", 12)                        # the round jacket and its rows of holes
        _ring_marks(v, -0.36, 0.13, 11, 0.0185, gz, (30.0, 90.0, 150.0, 210.0, 330.0), 0.022, 0.0105)
        v.lathe((0.17, 0.0, gz), X, [(0.13, 0.0), (0.13, 0.0185), (0.05, 0.0115), (0.045, 0.021), (0.0, 0.021), (0.0, 0.0)], "steel", v.n(12, 6))
        _bore(v, (0.30, 0.0, gz), X, 0.012)
        v.rod((-0.66, 0.0, gz), (-0.40, 0.0, gz), 0.0255, "steel", 12)                       # the receiver, round
        v.slab((-0.61, -0.016, gz - 0.05), (-0.47, 0.016, gz - 0.015), "steel", bevel=0.004)  # the trigger housing under it
        for x0, x1 in ((-0.63, -0.548), (-0.452, -0.43)):                                    # the feed cover's ends, the belt's way between
            v.slab((x0, -0.024, gz + 0.012), (x1, 0.024, gz + 0.041), "steel", bevel=0.003)
        v.extrude([(x, gz + z) for x, z in ((-0.632, 0.040), (-0.622, 0.058), (-0.47, 0.058), (-0.428, 0.047), (-0.428, 0.040))], -0.026, 0.026, "steel", bevel=0.004)
        butt = [(-0.66, 0.017), (-0.915, 0.015), (-0.922, -0.068), (-0.90, -0.076), (-0.83, -0.036), (-0.66, -0.019)]
        v.extrude([(x, gz + z) for x, z in butt], -0.017, 0.017, "bakelite", bevel=0.006)
        v.rod((-0.672, 0.0, gz), (-0.652, 0.0, gz), 0.0275, "steel", 8)
        if fine:
            v.slab((-0.40, -0.011, gz + 0.0185), (-0.34, 0.011, gz + 0.026), "steel", bevel=0.002)
            v.slab((-0.347, -0.010, gz + 0.026), (-0.341, 0.010, gz + 0.045), "steel")
            v.slab((0.105, -0.004, gz + 0.0185), (0.119, 0.004, gz + 0.05), "steel")
            v.rod((-0.50, -0.0255, gz - 0.004), (-0.50, -0.058, gz - 0.004), 0.006, "steel", 5)
            v.rod((-0.50, -0.058, gz - 0.004), (-0.50, -0.078, gz - 0.004), 0.011, "steel", 6)
            v.slab((0.09, -0.014, gz - 0.031), (0.125, 0.014, gz - 0.018), "steel", bevel=0.002)
    lo = gz - (0.028 if is42 else 0.05)
    v.extrude([(-0.535, lo), (-0.574, lo), (-0.606, lo - 0.092), (-0.57, lo - 0.098)], -0.0135, 0.0135, "bakelite", bevel=0.004)   # the gun's own grip
    if fine:
        v.tube([(-0.535, 0.0, lo - 0.002), (-0.505, 0.0, lo - 0.03), (-0.47, 0.0, lo - 0.002)], 0.003, "steel", 4)                # its trigger guard
    v.slab((-0.546, 0.0245, zf - 0.013), (-0.454, 0.070, zf - 0.007), "steel", bevel=0.002)     # the feed tray's lip, out to the left
    # The belt: in under the cover from the left, over the tray's lip, down to its box; the emptied links out at the right.
    _belt(v, [(0.012, zf), (0.072, zf), (0.105, zf - 0.035), (0.135, zf - 0.15), (0.165, 0.20), (0.185, 0.085)], "7.92", 0.015, "link", -0.50)
    _belt(v, [(-0.02, zf), (-0.055, zf - 0.006), (-0.078, zf - 0.05), (-0.085, zf - 0.15)], "7.92", 0.015, "link", -0.50, empty=True)
    v.socket("muzzle", (0.30, 0.0, gz), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.50, 0.07, gz + 0.025), Y, part="gun", note="the belt comes in here from the left: the number two's hands")
    v.socket("eject", (-0.50, 0.0, gz - 0.045), -Z, part="gun", note="the empties fall out underneath")
    v.socket("shoulder", (-0.92, 0.0, gz - 0.02), -X, part="gun", note="the butt: the gunner's right shoulder")
    v.use("mount")
    _belt_box(v, (-0.50, 0.265, 0.0), (0.36, 0.092, 0.17), "7.92", 0.015, "link", lid="end", far=1)
    v.socket("box", (-0.50, 0.265, 0.17), Z, part="mount", note="the belt box on the ground at the gun's left, end-on to her, its lid swung up at the far end")
    v.socket("seat", (-1.39, 0.20, 0.0), X, part="mount", note="the gunner's root, lying behind her a little to the left: his right shoulder at the butt 0.47 m ahead, his eye by the sight")
    v.socket("seat.loader", (-0.78, 0.62, 0.0), X, part="mount", note="the number two, lying at her left by the belt box")
    _controls(v, "prone", "left", burst="long bursts of 8 to 12 at 1,200 a minute (the MG 42; the MG 34 at 850), a pause, again")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The United States: the Browning M1919A4 on the M2 tripod (and, water-cooled, the M1917A1 pattern on its own).
# ---------------------------------------------------------------------------------------------------------------------
def browning(fine=True, kind="m1919"):
    """The Browning M1919A4 on the M2 tripod: a slab-sided receiver with its top cover, bolt handle and back plate, a
    barrel jacket full of round holes, a pistol grip sloping back, on a low tripod whose two rear legs carry the
    traversing bar and the elevating gear. The cloth belt comes up out of an M1 box at the left (its lid off, laid
    before it), every round drawn, in under the top cover; the emptied belt hangs out at the right. kind="m1917": the
    water-cooled pattern (a fat smooth jacket, its end caps, a filler and a steam hose to the can; a higher tripod):
    the M1917A1, and the Polish wz. 30 made from it."""
    air = kind == "m1919"
    v = _weapon("HmgM1919" if air else "HmgM1917", "Browning M1919A4 on the M2 tripod" if air else "water-cooled Browning on its tripod", fine,
                "4B4A32" if air else "4F5238", "4B4A32" if air else "4F5238")
    pz = 0.26 if air else 0.40
    gz = pz + 0.07
    spread = 1.0 if air else 1.15
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the pintle; the game places it")
    v.lathe((0.0, 0.0, pz - 0.07), Z, [(0.07, 0.0), (0.07, 0.03), (0.0, 0.034), (0.0, 0.0)], "mount", v.n(10, 6))
    hz = pz - 0.04
    fx = 0.44 * spread
    v.rod((0.03, 0.0, hz), (fx, 0.0, 0.02), 0.016, "mount", 6)
    v.rod((0.03, 0.0, hz), (0.03 + (fx - 0.03) * 0.25, 0.0, hz + (0.02 - hz) * 0.25), 0.021, "mount", 6)
    _foot(v, (fx, 0.0, 0.0), "claw")
    rear = (-0.56 * spread, 0.37 * spread)
    for s in (1, -1):
        a0, b0 = Vector((-0.03, s * 0.03, hz)), Vector((rear[0], s * rear[1], 0.02))
        v.rod(a0, b0, 0.016, "mount", 6)
        v.rod(a0, a0 + (b0 - a0) * 0.2, 0.021, "mount", 6)                                  # the leg's clamp at the head
        _foot(v, (rear[0], s * rear[1], 0.0), "shoe")
    # The traversing bar between the rear legs, its scale along the top.
    t = 0.64
    bx, by, bz = -0.03 + (rear[0] + 0.03) * t, 0.03 + (rear[1] - 0.03) * t, hz + (0.02 - hz) * t
    v.slab((bx - 0.012, -by, bz - 0.011), (bx + 0.012, by, bz + 0.011), "mount", bevel=0.002)
    if fine:
        v.slab((bx - 0.008, -by * 0.8, bz + 0.011), (bx + 0.008, by * 0.8, bz + 0.013), "steel")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-22.0, 22.0], kind="turn", note="swings on the pintle: + the muzzle to the right")
    v.lathe((0.0, 0.0, pz - 0.01), Z, [(0.04, 0.0), (0.04, 0.02), (0.0, 0.02), (0.0, 0.0)], "steel", v.n(8, 5))
    for s in (1, -1):                                                                      # the pintle's yoke either side of the receiver
        v.slab((-0.022, s * 0.027, pz + 0.01), (0.022, s * 0.035, gz - 0.02), "steel", bevel=0.002)
    # The traversing and elevating gear: the slide on the bar, the screw up to the receiver's back, its handwheel.
    ex = -0.27
    v.slab((bx - 0.022, -0.03, bz - 0.018), (bx + 0.022, 0.03, bz + 0.02), "steel", bevel=0.003)
    v.rod((bx, 0.0, bz + 0.02), (ex, 0.0, gz - 0.05), 0.011, "steel", 6)
    mid = Vector(((bx + ex) * 0.5, 0.0, (bz + gz - 0.05) * 0.5))
    _handwheel(v, mid - Z * 0.006, Vector((ex - bx, 0.0, gz - 0.07 - bz)), 0.03)
    if fine:
        v.rod((bx, 0.03, bz), (bx, 0.058, bz), 0.012, "steel", 6)                          # the traversing knob, at the left
        v.rod((bx, -0.03, bz + 0.005), (bx - 0.02, -0.07, bz + 0.03), 0.005, "steel", 4)   # the slide's clamp lever
    v.socket("grip.gunner.l", (-0.27 + (bx + 0.27) * 0.5, 0.03, (gz - 0.05 + bz) * 0.5), Y, part="cradle", note="the gunner's left hand on the elevating wheel")
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.02), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the pintle: + the muzzle up; and what is carried on a shoulder")
    zf = gz + 0.033                                                                         # the belt's rounds as they go in, under the cover
    v.box((0.36, 0.05, 0.10), (-0.16, 0.0, gz), "steel", bevel=0.005)                       # the receiver
    v.slab((-0.17, -0.027, gz + 0.05), (0.02, 0.027, gz + 0.062), "steel", bevel=0.003)     # the top cover
    v.slab((-0.354, -0.028, gz - 0.053), (-0.338, 0.028, gz + 0.053), "steel", bevel=0.003)  # the back plate
    v.rod((-0.34, 0.0, gz + 0.022), (-0.395, 0.0, gz + 0.022), 0.015, "steel", 8)           # its buffer tube
    v.extrude([(-0.343, gz - 0.022), (-0.372, gz - 0.022), (-0.412, gz - 0.128), (-0.376, gz - 0.134)], -0.014, 0.014, "wood", bevel=0.004)   # the pistol grip
    for sy in (1, -1):                                                                      # the belt's way through her, a dark slot each side
        _mark(v, (-0.05, sy * 0.025, zf), Y * sy, X, 0.094, 0.018)
    v.slab((-0.096, 0.025, zf - 0.013), (-0.004, 0.044, zf - 0.008), "steel", bevel=0.001)  # the lip the belt rides in on
    if fine:
        v.slab((-0.335, 0.002, gz + 0.05), (-0.27, 0.026, gz + 0.058), "steel", bevel=0.002)                # the rear sight's bed, its leaf up
        v.slab((-0.305, 0.006, gz + 0.058), (-0.299, 0.024, gz + 0.112), "steel")
        v.rod((-0.302, 0.026, gz + 0.066), (-0.302, 0.04, gz + 0.066), 0.006, "steel", 5)
        v.slab((-0.176, -0.012, gz + 0.062), (-0.156, 0.012, gz + 0.07), "steel", bevel=0.002)              # the cover's latch
        _mark(v, (-0.14, -0.025, gz + 0.006), -Y, X, 0.17, 0.008)                                            # the bolt handle's slot, and the handle
        v.rod((-0.085, -0.025, gz + 0.006), (-0.085, -0.056, gz + 0.006), 0.0055, "steel", 5)
        v.rod((-0.085, -0.056, gz + 0.006), (-0.085, -0.074, gz + 0.006), 0.01, "steel", 6)
        v.slab((-0.352, -0.005, gz - 0.02), (-0.366, 0.005, gz - 0.004), "steel")                            # the trigger
        v.slab((-0.33, -0.026, gz - 0.02), (-0.20, -0.0255, gz + 0.03), "flute")                             # the side plate's seam
    if air:
        jr, j1 = 0.022, 0.55
        v.rod((0.02, 0.0, gz + 0.005), (j1, 0.0, gz + 0.005), jr, "steel", 12)               # the jacket and its rows of holes
        _ring_marks(v, 0.07, 0.50, 10, jr, gz + 0.005, (30.0, 90.0, 150.0, 210.0, 330.0), 0.014, 0.014)
        v.rod((j1 - 0.022, 0.0, gz + 0.005), (j1, 0.0, gz + 0.005), jr + 0.0025, "steel", 12)   # its front cap, the barrel's bearing
        v.rod((j1, 0.0, gz + 0.005), (0.60, 0.0, gz + 0.005), 0.015, "steel", 8)
        v.slab((0.0, -0.004, gz + 0.062), (0.014, 0.004, gz + 0.098), "steel")               # the front sight, on the receiver's front
        v.slab((-0.01, -0.01, gz + 0.062), (0.022, 0.01, gz + 0.068), "steel")
        mx = 0.60
    else:
        jr, j1 = 0.047, 0.62
        v.rod((0.02, 0.0, gz + 0.005), (j1, 0.0, gz + 0.005), jr, "mount", 14)               # the water jacket
        for x0, x1 in ((0.02, 0.045), (j1 - 0.03, j1 + 0.004)):                               # its end caps
            v.rod((x0, 0.0, gz + 0.005), (x1, 0.0, gz + 0.005), jr + 0.004, "mount", 14)
        v.rod((j1, 0.0, gz + 0.005), (0.67, 0.0, gz + 0.005), 0.018, "steel", 8)
        v.rod((0.50, 0.0, gz + 0.045), (0.50, 0.0, gz + 0.072), 0.014, "brass", 6)           # the filler
        v.slab((0.55, -0.004, gz + 0.05), (0.57, 0.004, gz + 0.09), "steel")                 # the front sight, on the jacket
        v.slab((0.535, -0.012, gz + 0.048), (0.585, 0.012, gz + 0.056), "steel")
        if fine:
            v.rod((0.03, 0.0, gz + 0.052), (0.61, 0.0, gz + 0.052), 0.006, "mount", 5)       # the steam tube along the top
        mx = 0.67
    _bore(v, (mx, 0.0, gz + 0.005), X, 0.0075)
    v.socket("grip.gunner.r", (-0.365, 0.0, gz - 0.08), -X, part="gun", note="the gunner's right hand on the pistol grip")
    box_h = 0.185
    _belt(v, [(0.016, zf), (0.062, zf), (0.092, zf - 0.03), (0.118, zf - 0.12), (0.142, box_h + 0.05), (0.160, box_h - 0.09)], ".30", 0.018, "cloth", -0.05)
    _belt(v, [(-0.02, zf), (-0.052, zf - 0.006), (-0.072, zf - 0.05), (-0.078, zf - 0.14)], ".30", 0.018, "cloth", -0.05, empty=True)
    v.socket("muzzle", (mx, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.05, 0.06, gz + 0.03), Y, part="gun", note="the belt comes in here from the left: the number two's hands")
    v.socket("eject", (-0.05, 0.0, gz - 0.05), -Z, part="gun", note="the empties fall out underneath")
    v.use("mount")
    _belt_box(v, (-0.05, 0.235, 0.0), (0.28, 0.097, box_h), ".30", 0.018, "cloth", lid="off", far=1)
    v.socket("box", (-0.05, 0.235, box_h), Z, part="mount", note="the belt box on the ground at the gun's left, end-on to her, its lid off")
    if not air:                                   # the water can and its hose, at the right
        v.box((0.20, 0.13, 0.26), (0.30, -0.36, 0.13), "mount", bevel=0.01)
        if fine:
            v.rod((0.30, -0.36, 0.26), (0.30, -0.36, 0.285), 0.02, "steel", 8)
            v.tube([(0.24, -0.36, 0.26), (0.25, -0.36, 0.30), (0.35, -0.36, 0.30), (0.36, -0.36, 0.26)], 0.005, "steel", 4)
        v.tube([(0.56, -0.02, gz - 0.035), (0.52, -0.16, gz - 0.16), (0.36, -0.33, 0.32), (0.30, -0.36, 0.285)], 0.011, "rubber", 5)
    posture = "sit" if not air else "prone"
    v.socket("seat", (-1.13, 0.20, 0.0), X, part="mount",
             note="the gunner's root: lying behind her a little to the left, his right hand 0.77 m ahead on the pistol grip")
    v.socket("seat.loader", (-0.35, 0.62, 0.0), X, part="mount", note="the number two, lying at her left by the belt box")
    _controls(v, posture, "left", left=22.0, right=22.0, low=-10.0, high=14.0, burst="bursts of 6 to 9 at 450 a minute, a pause, again")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# Britain: the Vickers on its tripod.
# ---------------------------------------------------------------------------------------------------------------------
def _flutes(v, x0, x1, r, z, count=16):
    """A water jacket's lengthwise flutes: a darker line along it between each pair of ribs."""
    for k in range(count):
        a = 2.0 * math.pi * (k + 0.5) / count
        out = Vector((0.0, math.cos(a), math.sin(a)))
        _mark(v, Vector(((x0 + x1) / 2.0, 0.0, z)) + out * r, out, X, x1 - x0, 2.0 * math.pi * r / count * 0.42, "flute")


def vickers(fine=True):
    """The Vickers Mk I on the Mk IV tripod: a fat fluted water jacket with its end caps and muzzle cup, a slab breech
    casing with the feed block on top, the crank handle on its right, two spade grips, on a crosshead over two short
    front legs and a long rear one; the gunner sits behind astride the rear leg. The cloth belt comes up out of a box
    at the RIGHT, every round drawn, and through the feed block; the emptied belt hangs out at the left, and a hose
    runs from the jacket's nose to a condenser can at the left."""
    v = _weapon("HmgVickers", "Vickers Mk I on the Mk IV tripod", fine, "4A4D3A", "5A5540")
    pz, gz = 0.50, 0.60
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the crosshead; the game places it")
    v.lathe((0.0, 0.0, pz - 0.14), Z, [(0.12, 0.0), (0.12, 0.034), (0.0, 0.04), (0.0, 0.0)], "mount", v.n(10, 6))
    for s in (1, -1):
        a0, b0 = Vector((0.03, s * 0.04, pz - 0.10)), Vector((0.44, s * 0.39, 0.02))
        v.rod(a0, b0, 0.019, "mount", 6)
        v.rod(a0, a0 + (b0 - a0) * 0.16, 0.026, "mount", 6)                                 # the leg's socket and its jamming handle
        if fine:
            v.rod(a0 + (b0 - a0) * 0.08, a0 + (b0 - a0) * 0.08 + Vector((0.0, s * 0.05, 0.03)), 0.006, "steel", 4)
        _foot(v, (0.44, s * 0.39, 0.0), "shoe")
    a0, b0 = Vector((-0.03, 0.0, pz - 0.10)), Vector((-0.82, 0.0, 0.03))
    v.rod(a0, b0, 0.021, "mount", 6)
    v.rod(a0, a0 + (b0 - a0) * 0.14, 0.028, "mount", 6)
    _foot(v, (-0.82, 0.0, 0.0), "spade")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-30.0, 30.0], kind="turn", note="the crosshead turns in its socket: + the muzzle to the right")
    v.slab((-0.035, -0.045, pz - 0.03), (0.035, 0.045, pz + 0.03), "mount", bevel=0.004)
    for s in (1, -1):                                                                      # the crosshead's two cheeks up to the gun's pin
        v.slab((-0.02, s * 0.036, pz + 0.02), (0.02, s * 0.046, gz - 0.05), "mount", bevel=0.002)
    v.rod((-0.24, 0.0, gz - 0.065), (-0.10, 0.0, pz - 0.04), 0.013, "steel", 6)             # the elevating screw
    _handwheel(v, (-0.17, 0.0, pz - 0.012), Z, 0.05)
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.03), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the crosshead's pin: + the muzzle up; and what is carried on a shoulder")
    zf = gz + 0.08
    v.box((0.38, 0.066, 0.118), (-0.19, 0.0, gz), "steel", bevel=0.005)                     # the breech casing
    v.slab((-0.375, -0.028, gz + 0.059), (-0.115, 0.028, gz + 0.068), "steel", bevel=0.002)  # the rear cover
    jr = 0.05
    v.rod((0.0, 0.0, gz + 0.005), (0.64, 0.0, gz + 0.005), jr, "mount", 16)                 # the water jacket, fluted
    _flutes(v, 0.035, 0.605, jr, gz + 0.005)
    for x0, x1 in ((0.0, 0.03), (0.61, 0.645)):                                             # its end caps
        v.rod((x0, 0.0, gz + 0.005), (x1, 0.0, gz + 0.005), jr + 0.004, "mount", 16)
    v.rod((0.645, 0.0, gz + 0.005), (0.70, 0.0, gz + 0.005), 0.02, "steel", 8)
    v.lathe((0.70, 0.0, gz + 0.005), X, [(0.04, 0.0), (0.04, 0.03), (0.0, 0.034), (0.0, 0.0)], "steel", v.n(10, 6))   # the muzzle cup
    _bore(v, (0.74, 0.0, gz + 0.005), X, 0.012)
    v.rod((0.08, 0.0, gz + 0.05), (0.08, 0.0, gz + 0.075), 0.014, "brass", 6)               # the filler plug
    v.slab((-0.11, -0.05, gz + 0.059), (0.0, 0.05, gz + 0.105), "steel", bevel=0.004)        # the feed block
    for sy in (1, -1):
        _mark(v, (-0.055, sy * 0.05, zf), Y * sy, X, 0.09, 0.02)
    v.slab((-0.345, -0.014, gz + 0.068), (-0.325, 0.014, gz + 0.165), "steel")               # the rear sight's leaf, up
    v.slab((0.60, -0.004, gz + 0.054), (0.62, 0.004, gz + 0.09), "steel")                    # the foresight and its two wings
    if fine:
        for s in (1, -1):
            v.slab((0.592, s * 0.012, gz + 0.054), (0.628, s * 0.015, gz + 0.085), "steel")
        v.slab((-0.36, -0.02, gz + 0.068), (-0.31, 0.02, gz + 0.076), "steel", bevel=0.002)
        _mark(v, (-0.20, 0.033, gz - 0.005), Y, X, 0.26, 0.05, "flute")                       # the casing's side plates, let in
        _mark(v, (-0.14, -0.033, gz - 0.03), -Y, X, 0.14, 0.03, "flute")
    _crank(v, (-0.24, -0.033, gz + 0.012), -1)
    _spade_grips(v, -0.43, gz)
    v.socket("grip.gunner.l", (-0.43, 0.052, gz), -X, part="gun", note="the gunner's left hand on its spade grip")
    v.socket("grip.gunner.r", (-0.43, -0.052, gz), -X, part="gun", note="the gunner's right hand on its spade grip")
    box_h = 0.19
    _belt(v, [(-0.04, zf), (-0.078, zf), (-0.112, zf - 0.035), (-0.14, zf - 0.17), (-0.165, box_h + 0.06), (-0.185, box_h - 0.09)], ".303", 0.02, "cloth", -0.055)
    _belt(v, [(0.04, zf), (0.072, zf - 0.008), (0.09, zf - 0.06), (0.095, zf - 0.17)], ".303", 0.02, "cloth", -0.055, empty=True)
    v.socket("muzzle", (0.74, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.055, -0.085, gz + 0.08), -Y, part="gun", note="the belt comes in here from the RIGHT: the number two's hands")
    v.socket("eject", (-0.02, 0.0, gz - 0.07), -Z, part="gun", note="the empties fall out underneath")
    v.use("mount")
    _belt_box(v, (-0.055, -0.285, 0.0), (0.36, 0.10, box_h), ".303", 0.02, "cloth", lid="front", far=-1)
    v.socket("box", (-0.055, -0.285, box_h), Z, part="mount", note="the belt box on the ground at the gun's right, end-on to her, its lid up")
    v.box((0.24, 0.15, 0.32), (0.42, 0.40, 0.16), "mount", bevel=0.012)                       # the condenser can
    v.rod((0.42, 0.40, 0.32), (0.42, 0.40, 0.345), 0.022, "steel", 8)
    if fine:
        v.tube([(0.35, 0.40, 0.32), (0.36, 0.40, 0.365), (0.48, 0.40, 0.365), (0.49, 0.40, 0.32)], 0.005, "steel", 4)
    v.tube([(0.60, 0.02, gz - 0.04), (0.60, 0.16, gz - 0.17), (0.50, 0.33, 0.40), (0.44, 0.40, 0.345)], 0.012, "rubber", 5)
    v.socket("seat", (-0.78, 0.20, 0.0), X, part="mount", note="the gunner's root: sitting (today kneeling) behind her by the rear leg, his hands 0.35 m ahead on the grips")
    v.socket("seat.loader", (-0.30, -0.66, 0.0), X, part="mount", note="the number two, lying at her right by the belt box")
    _controls(v, "sit", "right", left=30.0, right=30.0, low=-10.0, high=14.0, burst="long steady bursts of 10 to 20 at 500 a minute, tapped across a front")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The Soviet Union: the Maxim of 1910 on Sokolov's wheeled mount, with its shield.
# ---------------------------------------------------------------------------------------------------------------------
def maxim(fine=True):
    """The Maxim M1910 on the Sokolov mount: a fat fluted water jacket with the wide filler cap of 1941, the feed block
    on top of the receiver, the crank handle on its right, spade grips, a steel shield with its top corners cut and a
    sighting slot, two small spoked steel wheels and a U-shaped trail the crew pull her by. The cloth belt comes up
    out of a green steel box at the RIGHT, outside the wheel, every round drawn, over the wheel and in through the
    feed block; the emptied belt hangs out at the left. The gunner sits (or lies) behind the trail's end."""
    v = _weapon("HmgMaxim", "Maxim M1910 on the Sokolov mount", fine, "4E5637", "4E5637")
    pz, gz, wr, wy = 0.37, 0.50, 0.205, 0.29
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the turntable, between her wheels; the game places it")
    v.rod((0.0, -wy - 0.03, wr), (0.0, wy + 0.03, wr), 0.018, "mount", 8)                     # the axle
    for s in (1, -1):
        y = s * wy
        ax = Y if s > 0 else -Y
        v.lathe((0.0, y - s * 0.02, wr), ax, [(0.0, wr - 0.03), (0.04, wr - 0.03), (0.04, wr), (0.0, wr), (0.0, wr - 0.03)], "mount", v.n(18, 9))   # the rim
        v.lathe((0.0, y - s * 0.025, wr), ax, [(0.05, 0.0), (0.05, 0.035), (0.0, 0.035), (0.0, 0.0)], "mount", v.n(8, 5))                           # the hub
        for k in range(8 if fine else 4):
            a = 2 * math.pi * k / (8 if fine else 4)
            v.rod((0.035 * math.cos(a), y, wr + 0.035 * math.sin(a)), ((wr - 0.03) * math.cos(a), y, wr + (wr - 0.03) * math.sin(a)), 0.007, "mount", 4)
        if fine:                                                                              # the earth her rim stands in
            v.slab((-0.06, y - s * 0.02 - 0.024, 0.0), (0.06, y - s * 0.02 + 0.024, 0.008), "earth")
    v.slab((-0.13, -0.11, wr + 0.015), (0.13, 0.11, wr + 0.05), "mount", bevel=0.005)          # the table
    v.lathe((0.0, 0.0, wr + 0.05), Z, [(pz - wr - 0.05, 0.0), (pz - wr - 0.05, 0.045), (0.0, 0.055), (0.0, 0.0)], "mount", v.n(10, 6))
    v.tube([(-0.06, 0.17, wr + 0.01), (-0.72, 0.13, 0.05), (-0.86, 0.0, 0.035), (-0.72, -0.13, 0.05), (-0.06, -0.17, wr + 0.01)], 0.017, "mount", 6)   # the trail
    v.rod((-0.86, -0.07, 0.035), (-0.86, 0.07, 0.035), 0.024, "wood", 8)                      # its handle
    if fine:
        v.rod((-0.40, -0.15, 0.134), (-0.40, 0.15, 0.134), 0.012, "mount", 5)                 # the trail's cross tie
    v.socket("pull", (-0.86, 0.0, 0.06), -X, part="mount", note="the trail's handle: where a man takes her to pull her along on her wheels")
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-35.0, 35.0], kind="turn", note="turns on the table: + the muzzle to the right (the shield turns with her)")
    v.slab((-0.05, -0.05, pz - 0.01), (0.05, 0.05, pz + 0.05), "mount", bevel=0.004)
    v.rod((-0.25, 0.0, gz - 0.07), (-0.17, 0.0, wr + 0.06), 0.013, "steel", 6)               # the elevating screw
    _handwheel(v, (-0.21, 0.0, (gz - 0.07 + wr + 0.06) / 2), Z, 0.045)
    # The shield: its top corners cut, the jacket's hole, a sighting slot above it.
    sx, top, bot, half, hole, th = 0.03, 0.74, 0.30, 0.27, 0.07, 0.007
    for s in (1, -1):
        cs = [Vector((sx, s * hole, bot)), Vector((sx, s * half, bot)), Vector((sx, s * half, top - 0.09)), Vector((sx, s * (half - 0.07), top)), Vector((sx, s * hole, top))]
        v.plate(cs if s < 0 else cs[::-1], th, "mount", bevel=0.002)
    v.slab((sx - th, -hole, gz + 0.075), (sx, hole, top), "mount")
    v.slab((sx - th, -hole, bot), (sx, hole, gz - 0.065), "mount")
    if fine:
        for face, out in ((sx, X), (sx - th, -X)):
            _mark(v, (face, 0.0, gz + 0.14), out, Z, 0.07, 0.014)
        for s in (1, -1):                                                                     # the two stays that hold it to the cradle
            v.rod((sx - th, s * 0.20, bot + 0.06), (-0.04, s * 0.05, pz + 0.03), 0.006, "mount", 4)
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.05), axis=(0, -1, 0), limits=[-8.0, 12.0], kind="turn", note="tips on her trunnions: + the muzzle up")
    zf = gz + 0.078
    v.box((0.34, 0.068, 0.118), (-0.17, 0.0, gz), "steel", bevel=0.005)                       # the receiver
    v.slab((-0.335, -0.029, gz + 0.059), (-0.105, 0.029, gz + 0.067), "steel", bevel=0.002)    # its cover
    jr = 0.055
    v.rod((0.0, 0.0, gz + 0.005), (0.66, 0.0, gz + 0.005), jr, "mount", 16)                  # the water jacket, fluted
    _flutes(v, 0.04, 0.625, jr, gz + 0.005)
    for x0, x1 in ((0.0, 0.03), (0.63, 0.665)):
        v.rod((x0, 0.0, gz + 0.005), (x1, 0.0, gz + 0.005), jr + 0.004, "mount", 16)
    v.rod((0.665, 0.0, gz + 0.005), (0.72, 0.0, gz + 0.005), 0.024, "steel", 8)
    v.lathe((0.72, 0.0, gz + 0.005), X, [(0.02, 0.0), (0.02, 0.019), (0.0, 0.024), (0.0, 0.0)], "steel", v.n(8, 5))
    _bore(v, (0.74, 0.0, gz + 0.005), X, 0.011)
    v.lathe((0.36, 0.0, gz + 0.05), Z, [(0.028, 0.0), (0.028, 0.045), (0.0, 0.045), (0.0, 0.0)], "mount", v.n(10, 6))   # the wide filler cap
    if fine:
        v.slab((0.31, -0.012, gz + 0.078), (0.33, 0.012, gz + 0.09), "steel")                 # its hinge and its catch
        v.slab((0.395, -0.008, gz + 0.066), (0.41, 0.008, gz + 0.082), "steel")
    v.slab((-0.10, -0.05, gz + 0.059), (0.0, 0.05, gz + 0.10), "steel", bevel=0.004)          # the feed block
    for sy in (1, -1):
        _mark(v, (-0.05, sy * 0.05, zf), Y * sy, X, 0.085, 0.02)
    v.slab((-0.30, -0.014, gz + 0.067), (-0.28, 0.014, gz + 0.15), "steel")                   # the rear sight
    if fine:
        v.slab((0.60, -0.004, gz + 0.059), (0.615, 0.004, gz + 0.09), "steel")                # the front sight
        _mark(v, (-0.18, 0.034, gz - 0.005), Y, X, 0.24, 0.05, "flute")
    _crank(v, (-0.22, -0.034, gz + 0.012), -1)
    _spade_grips(v, -0.39, gz)
    v.socket("grip.gunner.l", (-0.39, 0.052, gz), -X, part="gun", note="the gunner's left hand on its spade grip")
    v.socket("grip.gunner.r", (-0.39, -0.052, gz), -X, part="gun", note="the gunner's right hand on its spade grip")
    box_h = 0.19
    _belt(v, [(-0.04, zf), (-0.085, zf), (-0.17, zf - 0.03), (-0.27, zf - 0.09), (-0.345, zf - 0.17), (-0.385, box_h + 0.07), (-0.40, box_h - 0.09)],
          "7.62R", 0.022, "cloth", -0.05)
    _belt(v, [(0.04, zf), (0.072, zf - 0.008), (0.09, zf - 0.06), (0.095, zf - 0.17)], "7.62R", 0.022, "cloth", -0.05, empty=True)
    v.socket("muzzle", (0.74, 0.0, gz + 0.005), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (-0.05, -0.085, gz + 0.075), -Y, part="gun", note="the belt comes in here from the RIGHT: the number two's hands")
    v.socket("eject", (0.0, 0.0, gz - 0.07), -Z, part="gun", note="the empties fall out forward, under the jacket")
    v.use("mount")
    _belt_box(v, (-0.05, -0.50, 0.0), (0.28, 0.097, box_h), "7.62R", 0.022, "cloth", lid="front", far=-1)
    v.socket("box", (-0.05, -0.50, box_h), Z, part="mount", note="the belt box on the ground at her right, outside the wheel, end-on to her")
    v.socket("seat", (-0.74, 0.20, 0.0), X, part="mount", note="the gunner's root: sitting (today kneeling) inside the trail's loop, his hands 0.35 m ahead on the grips, the shield before him")
    v.socket("seat.loader", (-0.30, -0.80, 0.0), X, part="mount", note="the number two, lying at her right by the belt box")
    _controls(v, "sit", "right", left=35.0, right=35.0, burst="long steady bursts of 10 to 20 at 550 a minute",
              extra={"carry": {"what": "when the team moves she is not lifted: the game keeps her whole and rolls her behind the gunner on her wheels, her trail's handle (socket pull) in his hand; her wheels are 0.41 m across", "wheels": 0.41}})
    return v


# ---------------------------------------------------------------------------------------------------------------------
# Japan's Type 92, France's Hotchkiss of 1914 and Italy's Breda 37: finned barrels fed by strips, on tripods.
# ---------------------------------------------------------------------------------------------------------------------
def _strip(v, x_mid, y_in, y_out, z, cal, pitch, fed=0.0, empties=False):
    """A rigid feed strip of cartridges lying across the gun at x_mid, its rounds' bullets forward: the strip's plate
    from y_in (at the receiver's side) out to y_out, a round every `pitch` on it from its outer end inward. `fed`: the
    length of it that has gone through and sticks out at the other side (bare, or with `empties` the fired cases put
    back in it, as the Type 92 and the Breda do)."""
    L, case, sh, rb, rn = CARTRIDGES[cal]
    side = 1.0 if y_out > y_in else -1.0
    x0 = x_mid - L / 2.0
    lo, hi = min(y_in, y_out), max(y_in, y_out)
    v.slab((x0 + 0.004, lo, z - 0.004), (x0 + case - 0.006, hi, z), "brass" if not v.fine else "steel")
    if v.fine:
        for xa in (x0 + 0.008, x0 + case - 0.016):                                          # the strip's two rows of clips
            v.slab((xa, lo, z), (xa + 0.006, hi, z + rb * 1.2), "steel")
        n = int((abs(y_out - y_in) - 0.012) / pitch)
        for k in range(n):
            _round(v, (x0, y_out - side * (0.008 + pitch * k), z + rb), cal)
    else:
        v.slab((x0, lo, z), (x0 + L * 0.9, hi, z + rb * 1.6), "rounds")
    if fed > 0.0:
        a, b = -side * abs(y_in), -side * (abs(y_in) + fed)
        v.slab((x0 + 0.004, min(a, b), z - 0.004), (x0 + case - 0.006, max(a, b), z), "steel")
        if v.fine:
            for xa in (x0 + 0.008, x0 + case - 0.016):
                v.slab((xa, min(a, b), z), (xa + 0.006, max(a, b), z + rb * 1.2), "steel")
            if empties:
                for k in range(int((fed - 0.01) / pitch)):
                    _round(v, (x0, a - side * (0.012 + pitch * k), z + rb), cal, empty=True)


def strip_gun(fine=True, kind="type92"):
    """An air-cooled heavy machine gun fed by rigid strips of cartridges pushed in from the left, every round on the
    strip drawn: a slab receiver, a barrel with cooling fins, on a tripod of two front legs and one rear. type92:
    Japan's Type 92 (the Hotchkiss pattern made again: a strip of thirty, the fired cases put back in it as it comes
    out at the right; two grips that fold down, an oiler on the top, sockets on the legs for the carrying poles, a
    flash hider); hotchkiss: France's Mle 1914 (a strip of twenty-four that comes out bare, a pistol grip, five great
    rings by the receiver, a seat on the rear leg); breda37: Italy's Breda Mod. 37 (a strip of twenty, its fired cases
    put back; spade grips, a heavier finned barrel)."""
    name, title, mount, box = {
        "type92": ("HmgType92", "Type 92 heavy machine gun on its tripod", "4A4538", "6E5B3C"),
        "hotchkiss": ("HmgHotchkiss", "Hotchkiss Mle 1914 on its tripod", "4F5440", "5A5540"),
        "breda37": ("HmgBreda37", "Breda Mod. 37 on its tripod", "5B5F4A", "5B5F4A"),
    }[kind]
    v = _weapon(name, title, fine, mount, box)
    pz = {"type92": 0.34, "hotchkiss": 0.42, "breda37": 0.36}[kind]
    gz = pz + 0.10
    v.part("mount", None, (0, 0, 0), kind="root", note="on the ground under the pintle; the game places it")
    v.lathe((0.0, 0.0, pz - 0.10), Z, [(0.10, 0.0), (0.10, 0.03), (0.03, 0.06), (0.0, 0.06), (0.0, 0.0)], "mount", v.n(10, 6))
    hz = pz - 0.07
    for s in (1, -1):
        a0, b0 = Vector((0.04, s * 0.04, hz)), Vector((0.46, s * 0.37, 0.02))
        v.rod(a0, b0, 0.019, "mount", 6)
        v.rod(a0, a0 + (b0 - a0) * 0.16, 0.026, "mount", 6)
        _foot(v, (0.46, s * 0.37, 0.0), "shoe")
        if kind == "type92":                                  # the sockets the carrying poles go through
            v.rod((0.40, s * 0.37, 0.07), (0.52, s * 0.37, 0.07), 0.024, "mount", 8)
            _mark(v, (0.52, s * 0.37, 0.07), X, Z, 0.03, 0.03)
    a0, b0 = Vector((-0.04, 0.0, hz)), Vector((-0.74, 0.0, 0.03))
    v.rod(a0, b0, 0.02, "mount", 6)
    v.rod(a0, a0 + (b0 - a0) * 0.14, 0.027, "mount", 6)
    _foot(v, (-0.74, 0.0, 0.0), "spade")
    if kind == "type92":
        v.rod((-0.80, 0.0, 0.07), (-0.68, 0.0, 0.07), 0.024, "mount", 8)
        _mark(v, (-0.80, 0.0, 0.07), -X, Z, 0.03, 0.03)
    if kind == "hotchkiss":                                   # the gunner's seat on the rear leg
        v.lathe((-0.66, 0.0, 0.12), Z, [(0.02, 0.0), (0.02, 0.11), (0.0, 0.10), (0.0, 0.0)], "mount", v.n(10, 6))
        v.rod((-0.66, 0.0, 0.06), (-0.66, 0.0, 0.12), 0.012, "mount", 5)
    v.part("cradle", "mount", (0.0, 0.0, pz), axis=(0, 0, -1), limits=[-30.0, 30.0], kind="turn", note="turns on the pintle: + the muzzle to the right")
    v.slab((-0.04, -0.045, pz - 0.02), (0.04, 0.045, pz + 0.04), "mount", bevel=0.004)
    for s in (1, -1):
        v.slab((-0.02, s * 0.031, pz + 0.03), (0.02, s * 0.041, gz - 0.03), "mount", bevel=0.002)
    v.rod((-0.26, 0.0, gz - 0.06), (-0.12, 0.0, pz - 0.05), 0.012, "steel", 6)               # the elevating screw
    _handwheel(v, (-0.19, 0.0, pz - 0.015), Z, 0.045)
    v.part("gun", "cradle", (0.0, 0.0, pz + 0.04), axis=(0, -1, 0), limits=[-10.0, 14.0], kind="turn", note="tips on the pintle: + the muzzle up; and what is carried on a shoulder")
    rl = {"type92": 0.40, "hotchkiss": 0.44, "breda37": 0.42}[kind]
    v.box((rl, 0.06, 0.115), (-rl / 2, 0.0, gz), "steel", bevel=0.005)                        # the receiver
    v.slab((-rl + 0.02, -0.026, gz + 0.0575), (-0.03, 0.026, gz + 0.066), "steel", bevel=0.002)   # its top plate
    # The barrel: fins by the receiver, then plain to the muzzle.
    fins, f0, f1, hi, lo = {"type92": (9, 0.0, 0.40, 0.044, 0.033), "hotchkiss": (5, 0.0, 0.13, 0.05, 0.024), "breda37": (10, 0.02, 0.50, 0.036, 0.024)}[kind]
    bz = gz + 0.012
    if fine:
        pitch = (f1 - f0) / fins
        prof = [(f1, 0.0)]
        for k in range(fins, 0, -1):
            d = f0 + pitch * k
            prof += [(d, lo), (d - pitch * 0.25, lo), (d - pitch * 0.25, hi), (d - pitch * 0.7, hi), (d - pitch * 0.7, lo)]
        prof += [(f0, lo), (f0, 0.0)]
        v.lathe((0.0, 0.0, bz), X, prof, "steel", 10)
    else:
        v.rod((f0, 0.0, bz), (f1, 0.0, bz), (hi + lo) / 2, "steel", 8)
    ml = {"type92": 0.70, "hotchkiss": 0.80, "breda37": 0.68}[kind]
    v.rod((f1, 0.0, bz), (ml, 0.0, bz), 0.016 if kind != "hotchkiss" else 0.019, "steel", 8)
    if fine:                                                  # the gas cylinder under the barrel, its regulator at the front
        g1 = f1 + (ml - f1) * 0.55
        v.rod((0.0, 0.0, bz - 0.045), (g1, 0.0, bz - 0.045), 0.011, "steel", 6)
        v.slab((g1 - 0.012, -0.012, bz - 0.058), (g1 + 0.012, 0.012, bz + 0.004), "steel", bevel=0.002)
        v.slab((ml - 0.07, -0.004, bz + 0.014), (ml - 0.056, 0.004, bz + 0.05), "steel")       # the front sight
    if kind == "type92":
        v.lathe((ml, 0.0, bz), X, [(0.056, 0.0), (0.056, 0.025), (0.0, 0.015), (0.0, 0.0)], "steel", v.n(8, 5))   # the flash hider
        ml += 0.056
        for s in (1, -1):                                     # the two grips, folded down and out
            v.rod((-rl, s * 0.03, gz - 0.02), (-rl - 0.06, s * 0.06, gz - 0.11), 0.016, "wood", 8)
        gl, gr = (-rl - 0.04, 0.05, gz - 0.08), (-rl - 0.04, -0.05, gz - 0.08)
        v.box((0.07, 0.05, 0.035), (-rl * 0.45, 0.0, gz + 0.082), "steel", bevel=0.004)      # the oil tank on the top
        if fine:
            v.rod((-rl * 0.45, 0.0, gz + 0.0995), (-rl * 0.45, 0.0, gz + 0.112), 0.009, "brass", 6)
            v.slab((-rl - 0.012, -0.012, gz - 0.03), (-rl, 0.012, gz + 0.03), "steel")        # the trigger's thumb plate between the grips
    elif kind == "hotchkiss":
        v.extrude([(-rl + 0.012, gz - 0.0575), (-rl - 0.02, gz - 0.0575), (-rl - 0.055, gz - 0.16), (-rl - 0.018, gz - 0.165)], -0.015, 0.015, "wood", bevel=0.004)   # the pistol grip
        v.rod((-rl, 0.0, gz + 0.01), (-rl - 0.07, 0.0, gz + 0.01), 0.014, "steel", 6)        # the back handle
        if fine:
            v.rod((-rl - 0.07, -0.03, gz + 0.01), (-rl - 0.07, 0.03, gz + 0.01), 0.011, "wood", 6)
            v.tube([(-rl + 0.012, 0.0, gz - 0.06), (-rl + 0.05, 0.0, gz - 0.10), (-rl + 0.085, 0.0, gz - 0.06)], 0.003, "steel", 4)
        gl, gr = (-rl - 0.07, 0.0, gz + 0.01), (-rl - 0.01, 0.0, gz - 0.10)
    else:
        _spade_grips(v, -rl - 0.05, gz)
        gl, gr = (-rl - 0.05, 0.052, gz), (-rl - 0.05, -0.052, gz)
    _bore(v, (ml, 0.0, bz), X, 0.010 if kind != "type92" else 0.014)
    v.socket("grip.gunner.l", gl, -X, part="gun", note="the gunner's left hand")
    v.socket("grip.gunner.r", gr, -X, part="gun", note="the gunner's right hand, on the trigger")
    v.slab((-rl + 0.05, -0.012, gz + 0.066), (-rl + 0.07, 0.012, gz + 0.125), "steel")       # the rear sight
    if fine:
        v.rod((-0.20, -0.03, gz + 0.01), (-0.20, -0.058, gz + 0.01), 0.006, "steel", 5)       # the cocking handle at the right
        v.ball((-0.20, -0.066, gz + 0.01), 0.011, "steel", 6, 4)
        _mark(v, (-0.24, -0.03, gz + 0.01), -Y, X, 0.16, 0.008)
        _mark(v, (-rl * 0.62, 0.03, gz - 0.02), Y, X, rl * 0.5, 0.04, "flute")                # the receiver's side plate, let in
    # The strip: in from the left, every round on it; the fired end out at the right.
    fx = -0.10
    cal, pitch, fed, empties = {"type92": ("7.7", 0.013, 0.11, True), "hotchkiss": ("8L", 0.0155, 0.10, False), "breda37": ("8B", 0.0165, 0.055, True)}[kind]
    zs = gz + 0.026
    for sy in (1, -1):                                                                      # the feedway through the receiver
        _mark(v, (fx, sy * 0.03, zs + 0.005), Y * sy, X, 0.092, 0.022)
    v.slab((fx - 0.046, 0.03, zs - 0.012), (fx + 0.046, 0.075, zs - 0.004), "steel", bevel=0.002)   # the feed table the strip rides in on
    _strip(v, fx, 0.03, 0.33, zs, cal, pitch, fed=fed, empties=empties)
    v.socket("muzzle", (ml, 0.0, bz), X, part="gun", note="the muzzle: the flash and the tracer")
    v.socket("feed", (fx, 0.33, gz + 0.027), Y, part="gun", note="the strip is pushed in here from the left: the number two's hands, a new strip every 30 rounds")
    v.socket("eject", (fx, -0.13, gz + 0.02), -Y, part="gun", note="the spent strip comes out at the right")
    v.use("mount")
    _ammo_box(v, (-0.10, 0.52, 0.0), (0.42, 0.15, 0.12))
    if fine:                                                  # the next strip laid ready on the box
        L, case = CARTRIDGES[cal][0], CARTRIDGES[cal][1]
        rb = CARTRIDGES[cal][3]
        n = {"type92": 30, "hotchkiss": 24, "breda37": 20}[kind]
        x0, y0 = -0.10 - (n - 1) * pitch / 2.0, 0.52 - L / 2.0
        v.slab((x0 - 0.008, y0 + 0.004, 0.12), (x0 + (n - 1) * pitch + 0.008, y0 + case - 0.006, 0.124), "steel")
        for k in range(n):
            _round(v, (x0 + pitch * k, y0, 0.124 + rb), cal, way=Y, base=False)
    v.socket("box", (-0.10, 0.52, 0.12), Z, part="mount", note="the box of strips on the ground at her left, the next strip laid ready on it")
    v.socket("seat", (-rl - 0.82, 0.18, 0.0), X, part="mount", note="the gunner's root: sitting low (today lying) behind her a little to the left, his hands 0.78 m ahead on the grips")
    v.socket("seat.loader", (-0.25, 0.75, 0.0), X, part="mount", note="the number two, kneeling at her left with the next strip")
    _controls(v, "sit", "left", left=30.0, right=30.0, low=-10.0, high=14.0,
              burst="slow heavy bursts of 5 to 8 at 450 a minute; a strip is 30 rounds (a Hotchkiss's 24, a Breda's 20), then a beat while the next goes in")
    return v


# ---------------------------------------------------------------------------------------------------------------------
# The mortars: the Brandt pattern every army made.
# ---------------------------------------------------------------------------------------------------------------------
MORTARS = {
    # tag: (name, title, tube's length, bore's radius, baseplate (shape, long, wide), paint, the bombs' paint)
    "GER": ("MortarGER", "8 cm Granatwerfer 34", 1.14, 0.041, ("rect", 0.54, 0.38), "484A46", "4F2B25"),
    "USA": ("MortarUSA", "81 mm Mortar M1", 1.26, 0.041, ("rect", 0.50, 0.36), "4B4A32", "4B4A32"),
    "UK": ("MortarUK", "Ordnance ML 3-inch mortar", 1.19, 0.041, ("rect", 0.52, 0.40), "4A4D3A", "6A5A32"),
    "SOV": ("MortarSOV", "82 mm battalion mortar of 1937", 1.22, 0.041, ("round", 0.50, 0.50), "4E5637", "4E5637"),
    "JAP": ("MortarJAP", "Type 97 81 mm infantry mortar", 1.26, 0.041, ("rect", 0.50, 0.36), "5A5238", "2E2C2A"),
    "ITA": ("MortarITA", "Mortaio da 81 Mod. 35", 1.15, 0.041, ("rect", 0.50, 0.36), "5B5F4A", "5B5F4A"),
    "FRA": ("MortarFRA", "Brandt Mle 27/31, 81 mm", 1.26, 0.041, ("rect", 0.50, 0.36), "4F5440", "4F5440"),
}


def _bomb(v, at, heading, paint, r=0.039):
    """A mortar bomb lying on the ground: a teardrop body with its fuze, the tail tube with its ring of holes, six
    fins."""
    c, s = math.cos(heading), math.sin(heading)
    a = Vector((at[0], at[1], r + 0.004))
    ax = Vector((c, s, 0.0))
    v.lathe(a, ax, [(0.185, 0.011), (0.16, r * 0.62), (0.105, r), (0.04, r * 0.82), (0.0, r * 0.36), (0.0, 0.0)], paint, v.n(10, 5))
    v.lathe(a + ax * 0.185, ax, [(0.03, 0.0), (0.022, 0.008), (0.0, 0.011)], "brass", v.n(8, 5))          # the fuze
    v.rod(a - ax * 0.115, a, r * 0.3, "steel", 6)
    if v.fine:
        v.lathe(a + ax * 0.097, ax, [(0.012, r + 0.0012), (0.0, r + 0.0012)], "steel", 10)                 # the bourrelet's band
        for k in range(3):                                                                               # six fins: three plates through the tail
            ang = math.pi * k / 3.0
            rd = Vector((-s * math.cos(ang), c * math.cos(ang), math.sin(ang))) * (r * 0.92)
            p0, p1 = a - ax * 0.12, a - ax * 0.065
            cs = [p0 - rd, p1 - rd * 0.7, p1 + rd * 0.7, p0 + rd]
            nrm = (cs[1] - cs[0]).cross(cs[2] - cs[0]).normalized()
            v.plate([q + nrm * 0.0008 for q in cs], 0.0016, "steel")
    else:
        v.lathe(a - ax * 0.12, ax, [(0.05, 0.0), (0.05, r * 0.9), (0.0, r * 0.9), (0.0, 0.0)], "steel", 6)


def mortar(fine=True, tag="GER"):
    """A medium mortar of the Brandt pattern laid at about 62 degrees: the smooth tube (81 mm: slim, a ring at its
    mouth, its breech cap's ball in the baseplate), a ribbed baseplate with its carrying handle, a collar two thirds
    up the tube held through two shock absorbers by the yoke, the traversing screw and its handwheel across it, the
    elevating screw's housing down to where the two legs part, a cross-levelling link to the left leg, a chain
    between the legs, the sight on the yoke's left end. Three bombs with their fins and fuzes laid ready at the
    right. The gunner kneels at the left with his eye to the sight; the number two kneels at the right and drops the
    bombs in at the muzzle."""
    name, title, L, bore, (shape, pl, pw), paint, bomb = MORTARS[tag]
    v = _weapon(name, title, fine, paint, bomb)
    E = math.radians(62.0)
    d = Vector((math.cos(E), 0.0, math.sin(E)))
    under = Vector((math.sin(E), 0.0, -math.cos(E)))                                         # square to the tube, toward the ground ahead
    foot = Vector((0.0, 0.0, 0.075))
    tip = foot + d * L
    col = foot + d * (L * 0.62)
    r = bore + 0.006
    v.part("mortar", None, (0, 0, 0), kind="root", note="on the ground under the tube's foot (the baseplate's socket); the game places it")
    # The baseplate: leaning a little to meet the tube, its back edge dug in.
    lean = math.radians(14.0)
    n = Vector((math.sin(lean), 0.0, math.cos(lean)))
    u = Vector((math.cos(lean), 0.0, -math.sin(lean)))
    c = Vector((-0.02, 0.0, 0.04))
    if shape == "rect":
        v.plate([c - u * pl / 2 - Y * pw / 2, c + u * pl / 2 - Y * pw / 2, c + u * pl / 2 + Y * pw / 2, c - u * pl / 2 + Y * pw / 2], 0.035, "mount", bevel=0.008)
        if fine:
            for s in (1, -1):                                 # the ribs along it and the rim across its ends, the carrying handle
                for f in (0.16, 0.36):
                    a = c + n * 0.004 + Y * (s * pw * f)
                    v.plate([a - u * pl * 0.42 - Y * 0.009, a + u * pl * 0.42 - Y * 0.009, a + u * pl * 0.42 + Y * 0.009, a - u * pl * 0.42 + Y * 0.009], -0.012, "mount")
                e = c + n * 0.004 + u * (s * pl * 0.455)
                v.plate([e - u * 0.01 - Y * pw * 0.44, e + u * 0.01 - Y * pw * 0.44, e + u * 0.01 + Y * pw * 0.44, e - u * 0.01 + Y * pw * 0.44], -0.012, "mount")
            h = c - u * (pl / 2 - 0.03)
            v.tube([h - Y * 0.07, h - Y * 0.06 + n * 0.035, h + Y * 0.06 + n * 0.035, h + Y * 0.07], 0.006, "steel", 4)
    else:
        v.lathe(c - n * 0.035, n, [(0.035, 0.0), (0.035, pl / 2 - 0.02), (0.0, pl / 2), (0.0, 0.0)], "mount", v.n(18, 9))
        if fine:                                              # the round plate's ribs, like a wheel's spokes
            for k in range(8):
                ang = math.pi * k / 4.0 + math.pi / 8.0
                w = (u * math.cos(ang) + Y * math.sin(ang))
                sd = n.cross(w)
                a, b = c + n * 0.003 + w * 0.07, c + n * 0.003 + w * (pl / 2 - 0.03)
                v.plate([a - sd * 0.008, b - sd * 0.008, b + sd * 0.008, a + sd * 0.008], -0.011, "mount")
    v.ball(foot - d * 0.02, 0.05, "mount", 8, 5, dome=True)                                  # the socket's cup
    if fine:                                                  # the earth she is bedded in: packed under her raised back edge, thrown up before her front
        back, front = c - u * (pl * 0.5), c + u * (pl * 0.5)
        hw = pw * (0.47 if shape == "rect" else 0.36)
        v.extrude([(back.x - 0.05, 0.0), (back.x - 0.012, back.z - 0.036), (back.x + pl * 0.30, max(0.004, back.z - 0.036 - pl * 0.30 * math.tan(lean))), (back.x + pl * 0.36, 0.0)],
                  -hw, hw, "earth")
        v.extrude([(front.x - 0.03, 0.0), (front.x + 0.005, 0.022), (front.x + 0.03, 0.026), (front.x + 0.075, 0.0)], -hw, hw, "earth")
    # The bipod: the yoke's housing under the collar, the elevating screw up to it, two legs, the chain between them.
    yoke = col + Vector((0.10, 0.0, -0.16))
    feet = [Vector((yoke.x + 0.22, s * 0.36, 0.0)) for s in (1, -1)]
    for f in feet:
        v.rod(yoke, f + Z * 0.02, 0.016, "mount", 6)
        v.rod(yoke, yoke + (f - yoke) * 0.14, 0.022, "mount", 6)
        if fine:
            v.rod(f + Z * 0.045, f + Z * 0.0, 0.012, "steel", 5, r2=0.003)                    # the leg's spike
        _foot(v, (f.x, f.y, 0.0), "shoe")
    saddle = col + under * (r + 0.03)                                                        # where the screw meets the yoke under the tube
    v.rod(yoke - Z * 0.04, saddle, 0.02, "mount", 8)                                          # the elevating screw's housing
    v.rod(saddle - (saddle - yoke).normalized() * 0.11, saddle, 0.013, "steel", 6)            # the screw's bright thread out of it
    v.rod(yoke - Z * 0.02, yoke - Z * 0.02 - Y * 0.10, 0.008, "steel", 5)                     # its crank, out to the right
    v.rod(yoke - Z * 0.02 - Y * 0.10, yoke - Z * 0.07 - Y * 0.10, 0.008, "steel", 5)
    if fine:
        v.rod(yoke - Z * 0.07 - Y * 0.085, yoke - Z * 0.07 - Y * 0.125, 0.011, "wood", 6)
    m0, m1 = yoke + (feet[0] - yoke) * 0.5, yoke + (feet[1] - yoke) * 0.5
    v.rod(m0, m1, 0.006, "steel", 4)                                                          # the chain
    if fine:                                                  # the cross-levelling link: from the left leg up to the housing, its nut
        q0, q1 = yoke + (feet[0] - yoke) * 0.3, yoke + (saddle - yoke) * 0.55
        v.rod(q0, q1, 0.007, "steel", 5)
        qm = q0 + (q1 - q0) * 0.5
        v.rod(qm - (q1 - q0).normalized() * 0.02, qm + (q1 - q0).normalized() * 0.02, 0.014, "steel", 6)
    # The yoke: the traversing screw across under the tube, its handwheel; two shock absorbers up to the collar.
    side = Y
    v.rod(saddle - side * 0.12, saddle + side * 0.12, 0.014, "steel", 6)
    v.slab(tuple(saddle - Vector((0.03, 0.04, 0.03))), tuple(saddle + Vector((0.03, 0.04, 0.03))), "mount", bevel=0.004)
    _handwheel(v, saddle - side * 0.135, -Y, 0.035)
    for s in (1, -1):
        b0 = saddle + side * (s * 0.055)
        v.rod(b0 - d * 0.02, b0 + d * 0.15, 0.013, "mount", 6)
        v.rod(b0 + d * 0.15, b0 + d * 0.20, 0.007, "steel", 5)
        v.slab(tuple(b0 + d * 0.20 - Vector((0.012, 0.012, 0.012))), tuple(b0 + d * 0.20 - under * (0.03) + Vector((0.012, 0.012, 0.012))), "mount")
    # The sight on the yoke's left end: its stem, the body, the collimator along the top, its two drums.
    sight = col + side * 0.115 + Z * 0.10
    if fine:
        v.rod(saddle + side * 0.11, sight - Z * 0.03, 0.008, "steel", 5)
        v.box((0.045, 0.036, 0.05), tuple(sight - Z * 0.005), "steel", bevel=0.004)
        v.rod(sight + Z * 0.03 - X * 0.035, sight + Z * 0.03 + X * 0.035, 0.009, "steel", 6)
        _mark(v, sight + Z * 0.03 - X * 0.035, -X, Z, 0.013, 0.013, "glass")
        v.rod(sight + side * 0.018, sight + side * 0.034, 0.012, "steel", 6)
        v.rod(sight - Z * 0.005 - X * 0.0225, sight - Z * 0.005 - X * 0.036, 0.01, "steel", 6)
    v.socket("sight", tuple(col + side * 0.115 + Z * 0.10 - X * 0.03), -X, part="mortar", note="the sight on the yoke's left end: the gunner's eye comes to it from behind")
    v.socket("grip.traverse", tuple(col - side * 0.14), -Y, part="mortar", note="the traversing handwheel at the yoke's right end")
    v.socket("grip.elevate", tuple(yoke - Z * 0.07 - Y * 0.10), -Y, part="mortar", note="the elevating crank under the yoke, at the right")
    # The bombs laid ready and their case, at the right.
    for k in range(3):
        _bomb(v, (-0.10 + 0.02 * k, -0.62 - 0.11 * k, 0.0), math.radians(8.0 - 6.0 * k), "box")
    _ammo_box(v, (-0.45, -0.72, 0.0), (0.42, 0.26, 0.20), "mount")
    v.socket("rounds", (0.0, -0.72, 0.05), Z, part="mortar", note="the bombs laid ready: the number two's hand takes the next from here")
    v.socket("seat", (0.05, 0.45, 0.0), X, part="mortar", note="the gunner's root: kneeling at her left, his eye over the sight, his right hand to the collar")
    v.socket("seat.loader", (tip.x * 0.55, -0.50, 0.0), Vector((0.2, 1.0, 0.0)), part="mortar", note="the number two's root: kneeling at her right, facing the muzzle, a bomb in his hands")
    # The tube: its own part, so it can be laid, and carried on a shoulder.
    v.part("tube", "mortar", tuple(foot), axis=(0, -1, 0), limits=[-17.0, 23.0], kind="turn", note="the tube on its ball foot (rest: 62 degrees up): + steeper; and what is carried on a shoulder")
    v.lathe(foot, d, [(L, bore), (L, r + 0.004), (L - 0.035, r + 0.004), (L - 0.04, r), (0.10, r), (0.06, r + 0.012), (0.0, r * 0.7), (0.0, 0.0), (0.02, 0.0),
                      (0.02, bore * 0.6), (L - 0.02, bore), (L, bore)], "mount", v.n(14, 7))
    _mark(v, foot + d * (L - 0.05), d, Y, 2.0 * bore * 0.96, 2.0 * bore * 0.96, lift=0.0)        # the dark of her bore
    v.ball(foot - d * 0.03, 0.035, "steel", 8, 5)
    cb = col + d * 0.18                                                                       # the collar, where the shock absorbers hold her
    v.lathe(cb - d * 0.03, d, [(0.06, r), (0.06, r + 0.011), (0.0, r + 0.011), (0.0, r), (0.06, r)], "steel", v.n(14, 7))
    if fine:
        v.slab(tuple(cb + under * (r + 0.008) - Vector((0.016, 0.07, 0.016))), tuple(cb + under * (r + 0.008) + Vector((0.016, 0.07, 0.016))), "steel", bevel=0.003)
        for f in (0.30, 0.86):                                                                # two white lines to lay her by, a hair proud
            v.lathe(foot + d * (L * f), d, [(0.004, r + 0.0008), (0.0, r + 0.0008)], "flute", 14)
    v.socket("muzzle", tuple(tip), tuple(d), part="tube", note="the muzzle: the bomb is dropped in here, and the flash and the smoke come out")
    v.m.info.setdefault("file", {})["controls"] = {
        "elevate": {"part": "tube", "what": "the tube's lay (rest 62 degrees up; + steeper). The bipod is drawn for the rest lay: a change of a few degrees reads true, more wants the bipod moved", "limits": [-17.0, 23.0]},
        "gunner": {"posture": "kneel", "what": "the gunner kneels at her left, his eye to the sight (socket sight); the number two kneels at her right (seat.loader) and drops each bomb in at the muzzle, then ducks away with his hands to his ears"},
        "fire": {"what": "a bomb every 3 to 4 seconds when she fires for effect; one to range first. A thump, a ring of dust at the baseplate, a puff of grey smoke at the muzzle (no long flash by day)"},
        "carry": {"what": "when the team moves: the game shows the `tube` part alone on the gunner's shoulder and puts the baseplate and bipod away until the number two and the bearers have clips that carry them"},
    }
    return v


BUILDERS = {
    "HmgMG42": lambda fine=True: lafette(fine, "mg42"),
    "HmgMG34": lambda fine=True: lafette(fine, "mg34"),
    "HmgM1919": lambda fine=True: browning(fine, "m1919"),
    "HmgM1917": lambda fine=True: browning(fine, "m1917"),
    "HmgVickers": vickers,
    "HmgMaxim": maxim,
    "HmgType92": lambda fine=True: strip_gun(fine, "type92"),
    "HmgHotchkiss": lambda fine=True: strip_gun(fine, "hotchkiss"),
    "HmgBreda37": lambda fine=True: strip_gun(fine, "breda37"),
}
for _tag, _row in MORTARS.items():
    BUILDERS[_row[0]] = (lambda t: (lambda fine=True: mortar(fine, t)))(_tag)

ABOUT = {
    "HmgMG42": {"role": "heavy machine gun", "nation": "GER", "units": "German machine gun teams from the summer of 1942", "from": "1942-06"},
    "HmgMG34": {"role": "heavy machine gun", "nation": "GER", "units": "German machine gun teams until the MG 42 comes (and Romania's and Hungary's German-armed teams)", "to": "1942-05"},
    "HmgM1919": {"role": "heavy machine gun", "nation": "USA", "units": "American machine gun teams"},
    "HmgM1917": {"role": "heavy machine gun", "nation": "POL", "units": "Polish teams (the wz. 30), and any army's water-cooled Browning"},
    "HmgVickers": {"role": "heavy machine gun", "nation": "UK", "units": "British and Commonwealth machine gun teams"},
    "HmgMaxim": {"role": "heavy machine gun", "nation": "SOV", "units": "Soviet machine gun teams (and Finland's, on the same mount)"},
    "HmgType92": {"role": "heavy machine gun", "nation": "JAP", "units": "Japanese machine gun teams"},
    "HmgHotchkiss": {"role": "heavy machine gun", "nation": "FRA", "units": "French machine gun teams (and China's)"},
    "HmgBreda37": {"role": "heavy machine gun", "nation": "ITA", "units": "Italian machine gun teams"},
}
for _tag, _row in MORTARS.items():
    ABOUT[_row[0]] = {"role": "medium mortar", "nation": _tag, "units": _row[1]}
