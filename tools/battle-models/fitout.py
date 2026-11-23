"""What a battle adds to an aircraft of the game's.

The game's machines are sculpted to be seen on the map and on a card. A battle flies its camera a wing's length from
them, and at that distance a machine is known by what is in her and on her: the airman under the glass with his seat
and his sight, the exhaust stubs, the guns and their ports, the lamps at the wingtips, the gunner in his turret, the
bomb-aimer in the nose. This file adds those to a machine that planes.py has taken apart, each thing to the part it
rides on (so a gunner turns with his turret and a lamp goes with its wing).

Nothing here changes her shape, her colours or her markings. The hood and the turrets are still painted `glass`; a
battle should draw that paint clear (the file says how clear under `clear`), and then her crew are seen through it.

Everything is in the model's axes (y to port). The game's numbers have y to the right: `port()` turns one to the other.
"""
import math
import zlib

from mathutils import Matrix, Vector

import crewface

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# The paints of the fit-out (added after the game's own). `suit` and `vest` are set by her nation. `lens` (goggles, a
# reflector sight's glass, a bombsight's window) a smoked grey-blue, never brighter than his helmet's highlight: pale,
# it read as glowing eyes (the art director, 2026-11-21).
PAINTS = {"inside": "39423A", "leather": "3A2B20", "skin": "C59B7C", "suit": "5E5A46", "vest": "5E5A46", "goggles": "17181A", "exhaust": "4E3F36",
          "boot": "1B1B1D", "lamp_red": "C8201A", "lamp_green": "1FA35A", "lamp_white": "F0EBD8", "gunmetal": "26272A", "patch": "A32A22", "lens": "56636A"}
# The paints of the finer fit-out a machine's numbers ask for (`engine_face`, `blade_tips`: see below): her blades'
# yellow tips, the dull dark grey of an airscrew's hub, the plain disc on each blade where the maker's badge was, and a
# radial's cylinder heads as they are seen in the shade of the cowling (the art director, 2026-11-20: "finned and dark").
DETAIL_PAINTS = {"blade_tip": "C9A030", "hub": "4A4C4F", "decal": "9E3A2F", "decal_rim": "CFCABA", "head": "565A5D", "room": "58623F"}
# A glass nose whose room runs back past her side windows (her nose compartment's back wall, along her): its walls are
# lined, open where her windows are, in the paler green of a room the sun comes into, so that her nose reads as clear
# glass with a lit room behind it and her side windows look into it, not through her.
NOSE_ROOM = {"B17": 5.75}
# (flying suit, life jacket: the jacket is worn over the suit, and is yellow where it was)
SUITS = {"UK": ("4D5666", "C9A227"), "GER": ("5C6670", "A39A5C"), "USA": ("6E6247", "C9A227"), "JAP": ("5A4632", "6B5A3C"), "SOV": ("3F3B33", "4A4538"),
         "*": ("5E5A46", "6A6650")}
# Which machines carry guns over the engine, firing through the airscrew.
NOSE_GUNS = {"Bf109", "Zero", "Fw190", "Yak", "Fighter", "Dauntless", "Val"}
# Cannon in a jet's nose, each a port in her skin: (how far along her, where round her in degrees, 90 being her top).
NOSE_CANNON = {"Me262": [(3.78, 66.0), (3.78, 114.0), (3.62, 9.0), (3.62, 171.0)]}
# Who sits under each hood where it is not one pilot: (which hood, how far forward, how far to port, which way he looks).
# (The four carrier planes' are set by planes.carrier(), from carrier_dauntless.py, carrier_avenger.py, carrier_hellcat.py and
#  carrier_wildcat.py.)
SEATS = {"Kate": [(0, 0.85, 0.0, 0.0), (0, -0.45, 0.0, 0.0), (0, -1.75, 0.0, 180.0)], "Val": [(0, 0.75, 0.0, 0.0), (0, -1.4, 0.0, 180.0)],
         "DiveBomber": [(0, 0.7, 0.0, 0.0), (0, -1.3, 0.0, 180.0)], "TorpedoBomber": [(0, 1.15, 0.0, 0.0), (0, -0.3, 0.0, 0.0), (0, -1.7, 0.0, 180.0)],
         "Stuka": [(0, 0.55, 0.0, 0.0), (0, -0.85, 0.0, 180.0)], "Sturmovik": [(0, 0.7, 0.0, 0.0), (0, -1.25, 0.0, 180.0)],
         "He111": [(0, -0.45, 0.0, 180.0)], "Lancaster": [(0, 7.45, 0.3, 0.0), (0, 6.95, -0.3, 0.0), (0, 5.6, 0.0, 0.0)],
         "B17": [(0, 4.42, 0.36, 0.0), (0, 4.42, -0.36, 0.0)], "Bomber": [(0, 3.75, 0.28, 0.0), (0, 3.75, -0.28, 0.0)],
         "Betty": [(0, 5.4, 0.3, 0.0), (0, 5.4, -0.3, 0.0), (0, 3.9, 0.0, 0.0)], "Nell": [(0, 4.45, 0.24, 0.0), (0, 4.45, -0.24, 0.0)]}
# How far below the usual place (just under the glass) the men under a hood sit, where the hood is a tall cabin with
# windows in its sides and a roof over them.
SEAT_DROP = {"B17": 0.22}
# Who is seen through a glass nose: (forward, to port, height above the fuselage's middle or None for 'up under the glass', looks).
NOSE_CREW = {"He111": [(5.15, 0.3, None, 0.0), (6.25, -0.12, -0.05, 0.0)], "B17": [(7.72, 0.0, 0.05, 0.0)], "Bomber": [(5.65, 0.0, 0.0, 0.0)],
             "Betty": [(7.95, 0.0, -0.05, 0.0)], "Amerikabomber": [(7.35, 0.38, None, 0.0), (7.35, -0.38, None, 0.0), (8.7, 0.0, -0.25, 0.0)],
             "B29": [(8.75, 0.5, None, 0.0), (8.75, -0.5, None, 0.0), (9.95, 0.0, -0.3, 0.0)]}
# Where a bomb-aimer's head is in the near model, bent over his sight, where NOSE_CREW's place (for him upright) would
# put his helmet through her glass.
AIM_AT = {"B17": 7.6}


def port(v):
    return Vector((v[0], -v[1], v[2]))


def nation(s):
    tag = (s.get("side_marks") or [(0, 0, "*")])[0][2]
    return tag.split("-")[0]


def paints_for(s):
    out = dict(PAINTS)
    out["suit"], out["vest"] = SUITS.get(nation(s), SUITS["*"])
    if any(s.get(k) for k in ("engine_face", "blade_tips", "framed_windows", "fifties")):
        out.update(DETAIL_PAINTS)
    return out


def rivet_paints(paints):
    """A rivet is a faint shade under the paint it sits in (about a third as strong as a joint: the art director,
    2026-11-19, of the Val seen close: "real rivets read as a faint dotted line", not as studs), and the joint of two
    panels a full shade under it. On a black coat nothing is darker than the paint, so there they are a shade over it:
    worn edges and dust, as on a night machine."""
    from rigid import mix_hex

    def shade(c, k):
        r, g, b = (int(c[i:i + 2], 16) for i in (0, 2, 4))
        if 0.2126 * r + 0.7152 * g + 0.0722 * b < 44.0:
            return mix_hex(c, "A9A9A3", k * 0.6)
        return mix_hex(c, "000000", k)
    return {"rivet": shade(paints["upper"], 0.13), "rivet_under": shade(paints["under"], 0.11),
            "seam": shade(paints["upper"], 0.36), "seam_under": shade(paints["under"], 0.3)}


def nacelle_of(G, lead, z, is_radial, back, side, s=None):
    """An engine's nacelle on a wing, as the game's sculptor shapes it: round and stubby for an air-cooled engine, long
    and narrow for one in line. `nacelle` in her numbers is a nacelle of her own lines for an engine in line: stations
    (ahead of `lead`, above the airscrew's line, half its width, half its height), in the same frame as the game's
    (the spinner's tip 2.55 ahead of `lead`, its back at 2.09, the airscrew at 2.16); `nacelle_square` its section.
    `nacelles` gives each engine its own, by how far out it is: {out: stations} (the Lancaster's inner pair run back
    past her wing's trailing edge with her wheels in them; her outer pair end under her wing)."""
    if s and s.get("nacelles") and not is_radial:
        own = s["nacelles"][min(s["nacelles"], key=lambda out: abs(out - abs(side)))]
        return G["Body"]([(lead + dx, z + dz, w, h) for dx, dz, w, h in own], s.get("nacelle_square", 2.3), side)
    if s and s.get("nacelle") and not is_radial:
        return G["Body"]([(lead + dx, z + dz, w, h) for dx, dz, w, h in s["nacelle"]], s.get("nacelle_square", 2.3), side)
    if is_radial:
        return G["Body"]([(lead + 1.75, z, 0.46, 0.46), (lead + 1.65, z, 0.66, 0.66), (lead + 0.8, z, 0.68, 0.68), (lead - 0.4, z - 0.05, 0.62, 0.58),
                          (lead - back * 0.7, z - 0.1, 0.38, 0.32), (lead - back, z - 0.02, 0.08, 0.07)], 2.0, side)
    return G["Body"]([(lead + 2.55, z, 0.04, 0.04), (lead + 2.1, z, 0.34, 0.34), (lead + 2.05, z - 0.04, 0.4, 0.48), (lead + 0.6, z - 0.1, 0.47, 0.6),
                      (lead - 1.2, z - 0.14, 0.44, 0.5), (lead - back, z - 0.04, 0.1, 0.1)], 2.3, side)


def body_top(body, x, y=0.0):
    """How high the top of a round body (a fuselage, a hood) stands at x, y off its middle line."""
    w, h = body.w(-x), body.h(-x)
    e = body.square_at(x) if hasattr(body, "square_at") else 2.0 / body.power
    t = min(abs(y - body.side) / max(w, 1e-6), 1.0)
    return body.z(-x) + h * (1.0 - t ** e) ** (1.0 / e)


def hull_body(Body, s, fine=True):
    """Her fuselage's body. Round, as the game's are; or, with `taper` in her numbers, a rounded triangle: widest low
    down and drawn in toward her spine, as the Me 262's is (her wheels lie flat in the bottom of it). `taper` is (how
    much of her width is left at her spine, the height the narrowing begins at (-1 her keel, 0 her middle, 1 her
    spine), then stations (x, how much of the taper she has there: none at a round nose, all of it amidships)).
    Her stations' half widths are still her widest, as a plan view shows them. Round her, a point is still named by
    the same angle (its height is what the angle gives): `round_of` gives it back for a point of her skin.
    `round_below` (the Wildcat's): her belly rounder than the rest of her, below her middle: stations (x, how much of
    the way from her own section to a plain oval she is there below her middle: 0 none, 1 all of it); only her width
    at a height changes, so a height still names the same angle round her."""
    if s.get("cab"):
        return cab_hull(Body, s, fine)
    if s.get("squares"):
        return squared_hull(Body, s)
    if not s.get("taper"):
        return Body(s["hull"], s.get("square", 2.0))
    top, low, along = s["taper"]
    along = sorted(along, key=lambda r: -r[0])
    below = sorted(s.get("round_below") or [], key=lambda r: -r[0])
    widest = {}

    def oval_below(b, t, y):
        """Her width at height t (-1 her keel, 1 her spine) as a share of her widest, `y` her own section's, with `b`
        of the way to an oval below her middle."""
        if b > 0.0 and t < 0.0:
            return (1.0 - b) * y + b * math.sqrt(max(0.0, 1.0 - t * t))
        return y

    class Tapered(Body):
        def share(self, x):
            if x >= along[0][0]:
                return along[0][1]
            for (xa, ka), (xb, kb) in zip(along, along[1:]):
                if x >= xb:
                    return ka + (kb - ka) * (xa - x) / (xa - xb)
            return along[-1][1]

        def below(self, x):
            """How much of the way to an oval her section below her middle is here (`round_below`)."""
            if not below:
                return 0.0
            if x >= below[0][0]:
                return below[0][1]
            for (xa, ba), (xb, bb) in zip(below, below[1:]):
                if x >= xb:
                    return ba + (bb - ba) * (xa - x) / (xa - xb)
            return below[-1][1]

        def narrow(self, k, t):
            return 1.0 - (1.0 - top) * k * max(0.0, (t - low) / (1.0 - low))

        def widest(self, k, b=0.0):
            key = round(k, 3) if not b else (round(k, 3), round(b, 3))
            if key not in widest:
                n = 2.0 / self.power
                widest[key] = max(oval_below(b, t, (1.0 - abs(t) ** n) ** (1.0 / n)) * self.narrow(k, t) for t in (-1.0 + j / 60.0 for j in range(121)))
            return widest[key]

        def at(self, x, a, proud=0.0):
            c, sn = math.cos(a), math.sin(a)
            y, z = math.copysign(abs(c) ** self.power, c), math.copysign(abs(sn) ** self.power, sn)
            k, b = self.share(x), self.below(x)
            if b:
                y = math.copysign(oval_below(b, z, abs(y)), y)
            g = self.narrow(k, z) / self.widest(k, b)
            return Vector((x, self.side + y * (g * self.w(-x) + proud), self.z(-x) + z * (self.h(-x) + proud)))

        def round_of(self, p):
            """How far round her a point of her skin is: degrees up from level (90 on her spine, -90 on her keel)."""
            t = max(-1.0, min(1.0, (p.z - self.z(-p.x)) / max(self.h(-p.x), 1e-6)))
            return math.degrees(math.asin(math.copysign(abs(t) ** (1.0 / self.power), t)))

        def side_at(self, x, z):
            """How far out her side stands at this height (nothing above or below her)."""
            t = (z - self.z(-x)) / max(self.h(-x), 1e-6)
            if abs(t) >= 1.0:
                return 0.0
            n, k, b = 2.0 / self.power, self.share(x), self.below(x)
            if b:
                return self.w(-x) * oval_below(b, t, (1.0 - abs(t) ** n) ** (1.0 / n)) * self.narrow(k, t) / self.widest(k, b)
            return self.w(-x) * (1.0 - abs(t) ** n) ** (1.0 / n) * self.narrow(k, t) / self.widest(k)

        def top_at(self, x, y):
            """How high her upper skin stands at x, y off her middle line (her spine's height beyond her side)."""
            lo, hi = 0.0, 0.5 * math.pi
            if abs(y) >= self.at(x, lo).y:
                return self.z(-x)
            for _ in range(36):
                mid = 0.5 * (lo + hi)
                if self.at(x, mid).y > abs(y):
                    lo = mid
                else:
                    hi = mid
            return self.at(x, 0.5 * (lo + hi)).z
    return Tapered(s["hull"], s.get("square", 2.0))


def squared_hull(Body, s):
    """Her fuselage's body where its section is not one shape from nose to tail (`squares` in her numbers: (x, how
    square her section is there), in proportion between them; her `square` beyond the first and last): a round cowling
    in front, and slab sides under a long greenhouse whose rails stand on them, as the Devastator's do. Round her, a
    point is still named by the angle her `square` gives its height by, so that whatever finds a height on her by her
    `power` finds the same height; only how far out her side stands at that height follows the square there."""
    rows = sorted(s["squares"], key=lambda r: -r[0])

    class Squared(Body):
        def square_at(self, x):
            if x >= rows[0][0]:
                return rows[0][1]
            for (xa, ea), (xb, eb) in zip(rows, rows[1:]):
                if x >= xb:
                    return ea + (eb - ea) * (xa - x) / (xa - xb)
            return rows[-1][1]

        def at(self, x, a, proud=0.0):
            c, sn = math.cos(a), math.sin(a)
            t = math.copysign(abs(sn) ** self.power, sn)
            e = self.square_at(x)
            y = math.copysign((1.0 - min(abs(t), 1.0) ** e) ** (1.0 / e), c)
            return Vector((x, self.side + y * (self.w(-x) + proud), self.z(-x) + t * (self.h(-x) + proud)))
    return Squared(s["hull"], s.get("square", 2.0))


def cab_hull(Body, s, fine=True):
    """Her fuselage, round, with the cabin of flat windows her tail gunner sits in standing on it: a box, narrower than
    she is, on the round of her tail cone (her numbers' `cab`: (where she is still round ahead of it, where the box is
    whole, where it is still whole, where she is round again behind it, how high its flat roof stands, the degrees up
    from level its foot and its roof's edge stand at)). Brian, 2026-11-21, of the B-17: "The b-17 tail gunner windows
    should be a square box not a round bubble like it is"; the art director, of his second photo: a small, low box
    sitting on top of the rounded tail cone, about two thirds of the cone's width, with a ledge showing each side.
    Above her middle her skin is the outside of the two together: the cone, an oval as wide as she is and lower than
    her round (so that the box's foot stands on it at the foot's degrees), and the box, its sides as far out as puts
    its roof's edge at the roof's degrees. Round her a point is named by the angle it lies at from her middle, as
    before, so that a pane given in degrees up from level is where it looks to be (`pane_at`), and her rings, a ring
    every 9 degrees, stand on the box's foot and on its roof's edge: its corners are sharp. Below her middle she stays
    round. Between the round station and the box at each end her skin runs straight from one ring to the other, as her
    mesh does, so that what is laid on her skin there lies on it. Fore and aft of the cabin she is the body she was."""
    a0, a1, b1, b0, roof, foot, corner = s["cab"]
    if not fine:
        # (the table's model has a ring every 360/14 degrees: the box's foot and roof edge on the nearest of them, so that
        #  its corners are sharp there too, and its sides flat, not twisted between rings)
        grid = 360.0 / 14
        foot, corner = grid * round(foot / grid), grid * round(corner / grid)
    base = s.get("square", 2.0)
    t1, t2 = math.tan(math.radians(foot)), math.tan(math.radians(corner))
    lo, hi = math.radians(foot), math.radians(corner)

    class Cab(Body):
        def shape(self, x):
            """(how far out the box's sides stand, how high the cone stands), each as a share of her half width and
            half height there."""
            beta = (roof - self.z(-x)) / (self.h(-x) * t2)
            return beta, beta * t1 / math.sqrt(1.0 - beta * beta)

        def section(self, x, a, proud):
            c, sn = math.cos(a), math.sin(a)
            w, h, zc = self.w(-x), self.h(-x), self.z(-x)
            beta, kappa = self.shape(x)
            W, H = w + proud, h + proud
            b, k, rho = (beta * w + proud) / W, (kappa * h + proud) / H, (roof - zc + proud) / H
            cc = max(abs(c), 1e-9)
            r = max(1.0 / math.sqrt(cc * cc + (sn / k) ** 2), min(b / cc, rho / sn))
            return Vector((x, self.side + c * r * W, zc + sn * r * H))

        def at(self, x, a, proud=0.0):
            if math.sin(a) <= 0.0 or x >= a0 or x <= b0:
                return Body.at(self, x, a, proud)
            if a1 >= x >= b1:
                return self.section(x, a, proud)
            ends = (a0, a1) if x > a1 else (b0, b1)          # (round end, box end)
            k = (x - ends[0]) / (ends[1] - ends[0])
            return Body.at(self, ends[0], a, proud).lerp(self.section(ends[1], a, proud), k)

        def out(self, x, a):
            """Which way the skin faces there: across the round of her, from the face it is on (the cone, the box's
            side or its roof; at an edge, the one below it), so that what is laid along an edge lies flat on its face."""
            if math.sin(a) <= 0.0 or x >= a0 or x <= b0:
                return Body.out(self, x, a)
            up = a if math.cos(a) >= 0.0 else math.pi - a       # (degrees up from level, either side)
            lo_, hi_ = (0.0, lo) if up <= lo + 1e-9 else (lo, hi) if up <= hi + 1e-9 else (hi, 0.5 * math.pi)
            e = 0.01
            u0, u1 = max(up - e, lo_), min(up + e, hi_)
            if u1 - u0 < 1e-6:
                return Body.out(self, x, a)
            back = (lambda q: q) if math.cos(a) >= 0.0 else (lambda q: math.pi - q)
            ex = 0.01
            along = self.at(min(x + ex, self.front), a) - self.at(max(x - ex, self.back), a)
            round_ = self.at(x, back(u1)) - self.at(x, back(u0))
            n = round_.cross(along)
            if n.length < 1.0e-9:
                return Body.out(self, x, a)
            n.normalize()
            return n if n.dot(Vector((0.0, math.cos(a), math.sin(a)))) >= 0.0 else -n

        def roof_at(self, x):
            """How high the cabin's roof stands (none outside it)."""
            return roof if b1 - 1e-6 <= x <= a1 + 1e-6 else None
    return Cab(s["hull"], base)


def wall(m, hull, x, way, paint="inside", inset=0.012, sides=32):
    """A bulkhead across her fuselage at x, in her own section (a disc pokes through the sides of one that is not an oval)."""
    ring = [port(hull.at(x, 2.0 * math.pi * k / sides, -inset)) for k in range(sides)]
    mid = Vector((x, 0.0, hull.z(-x)))
    m.add(ring + [mid], [(k, (k + 1) % sides, sides) for k in range(sides)], paint, False, lambda q: Vector(way))


def cabin_body(Body, stations, cabin, hull=None):
    """The body of a raised cabin (her numbers' `cabin`). A hood is a round bubble of glass; a cabin is a box with its
    corners eased, so that its side windows are flat panes and its roof nearly flat, and its windscreen is two flat
    panes that meet at a post on her middle line and rake back to the roof (Brian, 2026-11-19, of the B-17: "see how
    the cock pit isn't round"). Behind its windows it rounds off again into the deck along her back.

    `square`: how square its section is where the windows are (2 is an ellipse, 4 nearly a box); `ease`: over how far
    behind `back` it comes round again; `screen`: the post between the two panes, (its foot's x and z, its head's x
    and z), then how far each pane sweeps back for each metre out from the post; `skirt`: (from this height, down to
    this one) the hollow between its sides and her fuselage's round side is filled (`fillet`: how fully), so that it
    grows out of her and does not sit on her as a box on a barrel."""
    box, soft, ease = cabin.get("square", 2.3), 2.3, cabin.get("ease", 1.0)
    skirt, fillet = cabin.get("skirt"), cabin.get("fillet", 0.08)
    (xa, za, xb, zb), sweep = cabin["screen"][:4], cabin["screen"][4]
    run = (xa - xb) / (zb - za)                           # (how far back a pane goes for each metre it rises)

    class Cabin(Body):
        def square_at(self, x):
            k = min(max((cabin["back"] - x) / ease, 0.0), 1.0)
            return box + (soft - box) * k * k * (3.0 - 2.0 * k)

        def flare(self, x, z, y):
            """How far out her side stands at this height, where the box's own wall stands `y` out. Under her windows
            the wall and her fuselage's round side would meet in a hollow: it is filled, as a fairing fills the root
            of a wing, so that the cabin grows out of her; lower still the cabin's skin lies just under her own."""
            if hull is None or not skirt or z >= skirt[0]:
                return y
            e = 2.0 / hull.power
            dz = abs(z - hull.z(-x)) / max(hull.h(-x), 1.0e-6)
            under = hull.w(-x) * (1.0 - dz ** e) ** (1.0 / e) - 0.012 if dz < 1.0 else 0.0      # (her fuselage's side at this height, and just under it)
            if z <= skirt[1]:
                return under
            t = (skirt[0] - z) / (skirt[0] - skirt[1])
            ease_in, ease_out = min(t / 0.12, 1.0), min(max((t - 0.7) / 0.3, 0.0), 1.0)
            ease_in, keep = ease_in * ease_in * (3.0 - 2.0 * ease_in), 1.0 - ease_out * ease_out * (3.0 - 2.0 * ease_out)
            fill = 0.5 * (math.sqrt((y - under) ** 2 + fillet * fillet) - abs(y - under))
            return under + (max(y, under) - under + fill * ease_in) * keep

        def box_at(self, x, a, proud=0.0):
            self.power = 2.0 / self.square_at(x)
            p = Body.at(self, x, a, proud)
            p.y = math.copysign(self.flare(x, p.z, abs(p.y)), p.y)
            return p

        def pane_z(self, x, y):
            """How high the windscreen's pane stands over this place."""
            return za + (xa - sweep * abs(y) - x) / run

        def pane_n(self, y):
            """The way the pane on that side faces (on her middle line: the way the post between them does)."""
            side = 0.0 if abs(y) < 0.002 else math.copysign(sweep / run, y)
            return Vector((1.0 / run, side, 1.0)).normalized()

        def holds(self, x, y, z):
            """Whether a place lies within the cabin there."""
            n = self.square_at(x)
            tz = abs(z - self.z(-x)) / max(self.h(-x), 1.0e-6)
            if tz >= 1.0:
                return False
            return abs(y) <= self.flare(x, z, self.w(-x) * (1.0 - tz ** n) ** (1.0 / n))

        def at(self, x, a, proud=0.0):
            p = self.box_at(x, a, proud)
            top = self.pane_z(x, p.y) + proud
            if p.z > top:
                p.z = top
            return p

        def cut_x(self, a):
            """Where the pane meets the box, this far round it: the windscreen's frame runs there. None if they do not meet."""
            def over(x):
                p = self.box_at(x, a)
                return p.z - self.pane_z(x, p.y)
            lo, hi = self.back, self.front                # (aft the box is under the pane, forward it is cut by it)
            if over(lo) > 0.0 or over(hi) < 0.0:
                return None
            for _ in range(40):
                mid = 0.5 * (lo + hi)
                if over(mid) > 0.0:
                    hi = mid
                else:
                    lo = mid
            return 0.5 * (lo + hi)

        def deg_of(self, x, z):
            """How far round the box (degrees up from level) its side stands at this height."""
            k = min(max((z - self.z(-x)) / max(self.h(-x), 1.0e-6), 0.0), 1.0)
            return math.degrees(math.asin(k ** (0.5 * self.square_at(x))))

        def xs(self, step):
            """Its rings: close together where the windscreen is, so that the edges of its panes are clean."""
            out = Body.xs(self, step)
            fine = min(step, 0.1) * 0.25
            x, lo = min(xa + 0.45, self.front), min(self.cut_x(math.radians(d)) or xb for d in range(20, 161, 10)) - 0.04
            out = [v for v in out if not lo < v < x]
            while x > lo:
                out.append(x)
                x -= fine
            return sorted(set(round(v, 5) for v in out if self.back <= v <= self.front), reverse=True)

    cab = Cabin(stations, box)
    cuts = [cab.cut_x(math.radians(d)) for d in range(30, 151, 5)]
    cab.roof_from = min(c for c in cuts if c is not None)   # (aft of this she has her whole roof)
    return cab


def hood_body(G, s, i=0):
    """The body of her i-th hood, as the sculptor shapes it: a round hood, or a raised cabin (her first, if her numbers give `cabin`)."""
    stations = s["hoods"][i][0]
    if i == 0 and s.get("cabin"):
        return cabin_body(G["Body"], stations, s["cabin"], G["Body"](s["hull"], s.get("square", 2.0)))
    return G["Body"](stations, 2.3)


def figure(m, head, face=0.0, vest=True, mask=True, chest=True, lean=0.0):
    """An airman as he is seen through glass: his head in its helmet and goggles, his neck and collar, his shoulders
    and his chest. `head` is the middle of his head; `face` the way he looks, in degrees to port of ahead; `lean` how
    far he bends forward from his hips, in degrees (a bomb-aimer over his sight). Returns a function that gives a place
    beside him: (ahead of his head, to his left, above it), leaning with him."""
    head = Vector(head)
    rot = Matrix.Rotation(math.radians(face), 3, "Z") @ Matrix.Rotation(math.radians(lean), 3, "Y")
    across = abs(math.sin(math.radians(face))) > 0.7          # (he sits across the machine)

    def at(dx, dy, dz):
        return head + rot @ Vector((dx, dy, dz))

    def r(a, b, c):
        return (b, a, c) if across else (a, b, c)
    u, v = (10, 6) if m.fine else (6, 4)
    if m.fine:                                                # (close: a real face under his helmet, crewface.py)
        seat = CREW["n"]
        CREW["n"] += 1
        goggles, masked = look_of(CREW["nation"], CREW["plane"], seat, mask)
        if lean:
            goggles = "up"                                    # (nobody sights through goggles: a bomb-aimer has his up on his helmet)
        crewface.head(m, at, CREW["nation"], CREW["plane"], seat, goggles=goggles, mask=masked)
    else:
        m.ball(at(0, 0, 0), r(0.105, 0.098, 0.118), "leather", u, v)
        m.ball(at(0.05, 0, -0.012), r(0.072, 0.076, 0.09), "skin", u, v)
    # his neck, from inside his chest up into his head, and his jacket's collar round its foot: head and body are balls
    # that would touch at a point only, with light all round between them (Brian, 2026-11-21: "make sure their necks
    # connect to their heads"). The neck about 12 to 13 cm across, the collar high, as a flying jacket's sits: only 2
    # to 4 cm of skin shows under the helmet (the art director)
    m.rod(at(-0.005, 0, -0.2), at(0.01, 0, -0.06), 0.064, "skin", 0.059, sides=10 if m.fine else 6)
    foot = at(-0.014, 0, -0.215) if m.fine and lean else at(-0.012, 0, -0.178)   # (bent forward, near: its foot well down in his chest,
    m.rod(foot, at(-0.008, 0, -0.106), 0.08, "suit", 0.072, sides=12 if m.fine else 6)     #  so that no skin shows between its rim and the chest's facets)
    if chest:
        m.ball(at(-0.02, 0, -0.33), r(0.15, 0.235, 0.2), "suit", u, v)
    else:
        m.ball(at(-0.02, 0, -0.24), r(0.11, 0.2, 0.12), "suit", u, v)                     # (only his shoulders, where there is no room for more of him)
    if vest and chest:
        m.ball(at(0.075, 0, -0.31), r(0.09, 0.165, 0.15), "vest", u, v)
    return at


# Who is being fitted out: her nation and name, and how many men have been seated (each man's face, goggles and mask
# are his seat's: the same every time she is built). `fit` sets it.
CREW = {"nation": "*", "plane": "", "n": 0}
# Machines that flew low, where a man wore his mask less often.
LOW = {"DiveBomber", "TorpedoBomber", "Stuka", "Sturmovik", "Val", "Kate", "Avenger", "Devastator", "Dauntless", "WhatIf_TankBuster", "WhatIf_AssaultGlider"}


def look_of(nation, plane, seat, mask=True):
    """His goggles (up on his brow, down over his eyes, or none) and whether he has his mask on."""
    h = zlib.crc32(f"look:{plane}:{seat}".encode())
    g = h % 10
    if nation == "JAP":                                       # (most Japanese airmen wore theirs pushed up: the art director)
        goggles = "up" if g < 7 else "down" if g < 9 else None
    else:
        goggles = "up" if g < 5 else "down" if g < 8 else None
    on = (h // 10) % 100 / 100.0 < (0.25 if plane in LOW else 0.3 if nation == "JAP" else 0.7)
    return goggles, bool(mask and on)


def arms(m, at, grips, fore=None, inside=None):
    """A gunner's two arms, from his shoulders to a grip for each hand (his left first; places on the machine): the
    upper arm, the elbow, the forearm and a gloved hand closed on the grip (Brian, 2026-11-19: "he should be in the
    turrett with 2 hands on the 50 call guns"). `fore`: the part his forearms and hands belong to, so that they stay
    on a gun that moves; his upper arms stay with him. `inside`: where a narrow fuselage has room for his upper arm (a
    function of the place his elbow would hang and his shoulder, giving the place it may hang)."""
    body = m.parts[m.cur].name
    u, v = (8, 5) if m.fine else (6, 3)
    for sy, grip in zip((1.0, -1.0), grips):
        grip = Vector(grip)
        sh = at(-0.02, 0.2 * sy, -0.25)
        out = (at(0.0, sy, 0.0) - at(0.0, 0.0, 0.0)).normalized()
        reach = (grip - sh).length
        sag = math.sqrt(max(0.29 ** 2 - (0.5 * reach) ** 2, 0.0))      # (an arm is two bones of 29 cm: the elbow hangs as far off the line as they allow)
        elbow = (sh + grip) * 0.5 + (out * 0.55 - Z * 0.83).normalized() * sag
        if inside:
            elbow = inside(elbow, sh)
        m.use(body)
        if m.fine:                                       # (near: a flying jacket's sleeve, full at the shoulder and round at the
            m.rod(sh, elbow, 0.066, "suit", 0.058, sides=9)    #  elbow, so that his arm reads as one limb however far it bends)
            m.ball(elbow, 0.062, "suit", u, v)
        else:
            m.rod(sh, elbow, 0.06, "suit", 0.05, sides=7)
            m.ball(elbow, 0.052, "suit", u, v)
        if fore:
            m.use(fore)
        m.rod(elbow, grip, 0.056 if m.fine else 0.048, "suit", 0.042 if m.fine else 0.04, sides=9 if m.fine else 7)
        m.ball(grip, (0.05, 0.045, 0.05), "leather", u, v)
    m.use(body)


def seats_of(f, s, G):
    """Everyone under a hood: (the hood's curves, forward, to port, looks)."""
    hoods = [hood_body(G, s, i) for i in range(len(s.get("hoods", [])))]
    if f.name in SEATS:
        return [(hoods[i], x, y, face) for i, x, y, face in SEATS[f.name] if i < len(hoods)]
    return [(h, h.front - max(0.8, 0.38 * (h.front - h.back)), 0.0, 0.0) for h in hoods[:1]]


def cockpit(m, f, s, G, hull):
    """Under each hood: the dark of the cockpit, and whoever sits there with his seat; a fighter's sight and coaming."""
    m.use("plane")
    for i, (stations, hoops, spine) in enumerate(s.get("hoods", [])):
        if getattr(f, "cockpit", None):
            open_cockpit(m, f, s, hull, G)                 # (her cockpit is open under her canopy: its own dark, and what is in it)
            break
        hood = hood_body(G, s, i)
        n = 14 if m.fine else 6
        first = s["cabin"]["screen"][0] - 0.22 if i == 0 and s.get("cabin") else hood.front - 0.12      # (a cabin's windscreen stands well aft of its body's nose)
        xs = [first - (first - hood.back - 0.12) * k / n for k in range(n + 1)]
        half = 0.8 * max(r[2] for r in stations)
        m.strip([port(hull.at(x, math.pi / 2.0)) for x in xs], [port(hull.out(x, math.pi / 2.0)) for x in xs], half, "inside", lift=0.012,
                taper=lambda t: 0.45 + 0.55 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.5)
    single = not s.get("engines")
    first = True
    for hood, x, y, face in seats_of(f, s, G):
        z = body_top(hood, x, -y) - 0.118 - 0.03 - SEAT_DROP.get(f.name, 0.0)
        at = figure(m, (x, y, z), face, vest=single)
        f.seated = getattr(f, "seated", []) + [(Vector((x, y, z)), face, at)]             # (for rear_gunner: a man facing aft at a gun)
        if not m.fine:
            continue
        m.box(at(-0.2, 0, -0.3), (0.05, 0.42, 0.36), "inside", yaw=face)                    # the back of his seat
        if single and face == 0.0 and s.get("canopy"):
            xa = x - 0.185                                                                  # the armour behind his head, clear of the glass
            hi = min(z + 0.15, min(body_top(hood, xa, -y + d) for d in (-0.135, 0.0, 0.135)) - 0.035)
            m.box((xa, y, 0.5 * (z - 0.15 + hi)), (0.02, 0.27, hi - z + 0.15), "gunmetal")
            if first:
                controls(m, at, x, z, hull)
        elif single and face == 0.0:
            m.box(at(-0.185, 0, 0.0), (0.02, 0.27, 0.3), "gunmetal")                        # the armour behind his head
        if not single and first and face == 0.0 and s.get("pilot_wheel"):
            wheel(m, at, hull.z(-x) - 0.36 * hull.h(-x))    # (a bomber's pilot in her open cabin: both his hands on his wheel)
        if single and first and face == 0.0 and not getattr(f, "cockpit", None):
            sill = hull.at(x + 0.56, math.pi / 2.0).z
            if x + 0.75 < hood.front:
                wide = min(0.85 * hood.w(-(x + 0.56)), 0.25)
                m.ball((x + 0.56, 0.0, sill), (0.16, min(wide, 0.2), 0.075), "gunmetal", 12, 6)   # the coaming over his instruments
                m.box((x + 0.43, 0.0, z - 0.03), (0.06, 0.05, 0.08), "gunmetal")            # his sight, and its glass
                m.box((x + 0.46, 0.0, z + 0.04), (0.008, 0.075, 0.085), "lens", pitch=-32.0)
        first = False


def open_cockpit(m, f, s, hull, G=None):
    """A fighter's cockpit, open under her canopy (planes.py cuts her skin away there: fighters.opening): her skin
    again from inside, dark; her instrument panel across the front of it with its dials, and a bulkhead behind her
    seat; a floor; and a padded rim round its edge (Brian, 2026-11-19: the crew "fully inside with their hands on
    the controls")."""
    x0, x1, edge = f.cockpit
    liner(m, f, hull, x0, x1, lambda x, high: x1 < x < x0 and high > edge)
    for x, way in ((x0, -X), (x1, X)):
        if hasattr(hull, "round_of"):
            wall(m, hull, x, way)                          # (a fuselage that is not an oval: fitout.hull_body)
        else:
            m.disc((x, 0.0, hull.z(-x)), way, hull.h(-x) * 0.97, "inside", 24, oval=hull.w(-x) / hull.h(-x))
    xm = 0.5 * (x0 + x1)
    m.box((xm, 0.0, hull.z(-xm) - 0.36 * hull.h(-xm)), (x0 - x1, 1.5 * hull.w(-xm), 0.04), "inside")              # (her floor)
    if s.get("opening_plain"):
        open_rim(m, f, hull, x0, x1, edge)                 # (a gunner's position: no panel, no coaming, no sight)
        return
    if s.get("opening_cabin"):
        # (a bomber's cabin, open under its greenhouse from her sills up: her instrument panel across under the foot
        #  of her windscreen with its rows of dials; no sight, and no padded rim: the greenhouse's own rail stands on
        #  the edge of her skin)
        top = hull.at(x0, math.pi / 2.0).z
        m.box((x0 - 0.03, 0.0, top - 0.2), (0.025, 1.24, 0.34), "gunmetal")
        if m.fine:
            for row, dz in enumerate((-0.11, -0.25)):
                for k in range(8):
                    m.disc((x0 - 0.044, (k - 3.5) * 0.13 + (0.04 if row else 0.0), top + dz), -X, 0.04, "lens", 12)
        return
    # (her instrument panel, black, its top under the coaming, with two rows of dials)
    lip = hull.at(x0, math.radians(edge)).z
    wide = hull.at(x0, math.radians(edge)).y
    m.box((x0 - 0.03, 0.0, lip - 0.17), (0.025, 2.0 * wide - 0.04, 0.3), "gunmetal")
    if m.fine:
        for row, dz in enumerate((-0.09, -0.21)):
            for k in range(4):
                dy = (k - 1.5) * 0.105 + (0.03 if row else 0.0)
                m.disc((x0 - 0.044, dy, lip + dz), -X, 0.036, "lens", 12)
    # (the coaming over it, dark, back from the foot of her windscreen; her gunsight on it, and its glass)
    foot = max(q[0] for q in s["hoods"][0][0])
    cx = [x0 + (foot - x0) * k / 8.0 for k in range(9)]
    m.strip([port(hull.at(x, math.pi / 2.0)) for x in cx], [port(hull.out(x, math.pi / 2.0)) for x in cx], 0.16, "inside", lift=0.008,
            taper=lambda t: 1.0 - 0.45 * t)
    if m.fine and s.get("sight") != "telescope":            # (`sight`: 'telescope' is a telescope sight of her own, in her extras, and no reflector)
        sx = x0 + 0.1
        top = hull.at(sx, math.pi / 2.0).z
        if G is not None:
            top = min(top, body_top(hood_body(G, s, 0), sx + 0.02) - 0.2)     # (and under her windscreen's glass, where it is low over it)
        m.box((sx, 0.0, top + 0.045), (0.08, 0.055, 0.09), "gunmetal")
        m.box((sx + 0.02, 0.0, top + 0.12), (0.008, 0.075, 0.085), "lens", pitch=-32.0)
    open_rim(m, f, hull, x0, x1, edge)


def open_rim(m, f, hull, x0, x1, edge):
    """The rim round the edge of an open cockpit, padded."""
    xs = [x for x in sorted(f.rings_x, reverse=True) if x1 - 1e-4 <= x <= x0 + 1e-4]
    for a in (edge, 180.0 - edge):
        m.strip([port(hull.at(x, math.radians(a))) for x in xs], [port(hull.out(x, math.radians(a))) for x in xs], 0.026, "leather", lift=0.006)
    arc = [edge + (180.0 - 2.0 * edge) * k / 8.0 for k in range(9)]
    for x in (x0, x1):
        m.strip([port(hull.at(x, math.radians(a))) for a in arc], [port(hull.out(x, math.radians(a))) for a in arc], 0.026, "leather", lift=0.006)


def controls(m, at, x, z, hull):
    """A fighter pilot in his seat with his hands on his controls: his right on the stick between his knees, his left
    on the throttle at his left side; his seat under him and his thighs on it."""
    floor = hull.z(-x) - 0.36 * hull.h(-x)
    stick = at(0.38, -0.02, -0.52)
    m.rod(Vector((stick.x + 0.03, stick.y, floor)), stick - Z * 0.03, 0.016, "gunmetal", sides=6)                 # the stick
    m.box(stick + Z * 0.02, (0.035, 0.035, 0.1), "leather")                                                      # its grip
    throttle = at(0.27, 0.25, -0.43)
    m.box(at(0.2, 0.3, -0.5), (0.26, 0.05, 0.08), "gunmetal")                                                     # the throttle's quadrant on the wall
    m.rod(at(0.18, 0.29, -0.48), throttle, 0.012, "gunmetal", sides=5)                                            # its lever
    arms(m, at, (throttle, stick))
    lap(m, at)


# A man's bones, in metres (a man of about 1.78 m): from the middle of his head down to his hip joints as he stands and
# as he sits, his thigh and his shin. (Brian, 2026-11-21: "the bombadier in the window doesnt have a body"; "can you see
# the tail and waist gunners too?": every airman seen through glass or an open window is a whole man, down to his boots.)
HIP_STAND, HIP_SIT, THIGH, SHIN = 0.78, 0.72, 0.43, 0.42
AIM_LEAN = 35.0                                          # (a bomb-aimer bent over his sight, his eye to it)


def _ahead(at):
    """The way he faces, level, and to his left (unit vectors on the machine), however far he leans."""
    o = at(0.0, 0.0, 0.0)
    way = at(1.0, 0.0, 0.0) - o
    way.z = 0.0
    return way.normalized(), (at(0.0, 1.0, 0.0) - o).normalized()


def upright(at, face):
    """The frame of the same man sitting upright on the same hips: for his seat, his legs and what stands on the floor
    in front of him, when he leans over it."""
    hip = at(-0.02, 0.0, -HIP_SIT)
    rot = Matrix.Rotation(math.radians(face), 3, "Z")
    return lambda dx, dy, dz: hip + rot @ Vector((dx + 0.02, dy, dz + HIP_SIT))


def _r(at, a, b, c):
    """A ball's radii (ahead, across, up) as the machine's axes have them: swapped when he faces across her."""
    way, _ = _ahead(at)
    return (b, a, c) if abs(way.y) > 0.7 else (a, b, c)


def bottom_at(hull, x, y=0.0):
    """How low the inside of her skin is at x, y off her middle line (her floor can lie no lower)."""
    return 2.0 * hull.z(-x) - body_top(hull, x, y)


def trunk(m, at, sit=True):
    """His middle, from the foot of his chest down to his seat: his belly and his hips, so that his chest does not end
    in the air."""
    u, v = (10, 6) if m.fine else (6, 4)
    hz = -HIP_SIT if sit else -HIP_STAND
    m.ball(at(-0.035, 0.0, 0.5 * (hz - 0.33) - 0.02), _r(at, 0.135, 0.2, 0.5 * (-0.33 - hz) + 0.04), "suit", u, v)
    m.ball(at(-0.05, 0.0, hz + 0.02), _r(at, 0.145, 0.2, 0.12), "suit", u, v)


def _boot(m, ankle, way, flat=False, inside=None):
    """A flying boot round his foot: heel under his ankle, toe out ahead of it (or lying flat behind him, `flat`); where
    her skin curves up under it (`inside`), his toe turned up against it."""
    if flat:
        m.rod(ankle + way * 0.04, ankle - way * 0.2 - Z * 0.02, 0.06, "boot", 0.045, sides=7)
        return
    toe = ankle + way * 0.19 - Z * 0.05
    if inside:
        toe = inside(toe)
    m.rod(ankle - way * 0.05 + Z * 0.02, toe, 0.065, "boot", 0.048, sides=7)
    m.ball(ankle, 0.06, "boot", 6, 4)


def legs(m, at, pose, floor, spread=0.15, inside=None):
    """His legs down to the floor at height `floor` (on the machine): "sit" on a seat ahead of him, "stand" at his gun,
    or "kneel" on pads with his shins back along the floor. Two bones each, the knee where they meet: when the floor is
    nearer than a straight leg the knee bends (ahead as he sits or stands, to the floor as he kneels). `inside`: a
    function giving the place his foot may stand at (so that a boot never goes through her skin). Returns his knees."""
    way, left = _ahead(at)
    u, v = (8, 5) if m.fine else (6, 3)
    knees = []
    for sy in (1.0, -1.0):
        if pose == "stand":
            hip = at(-0.03, 0.1 * sy, -HIP_STAND)
            ankle = hip + left * 0.04 * sy
            ankle.z = floor + 0.09
            if inside:
                ankle = inside(ankle)
            d = (ankle - hip).length
            if d > THIGH + SHIN:                         # (too high above her floor for his legs: they hang straight)
                ankle = hip + (ankle - hip).normalized() * (THIGH + SHIN)
                d = THIGH + SHIN
            a = (d * d + THIGH * THIGH - SHIN * SHIN) / (2.0 * d)
            bend = math.sqrt(max(THIGH * THIGH - a * a, 0.0))
            down = (ankle - hip).normalized()
            fwd = (way - down * way.dot(down)).normalized()
            knee = hip + down * a + fwd * bend
            foot = way
        elif pose == "sit":
            hip = at(-0.02, 0.1 * sy, -HIP_SIT)
            rise = max(min(hip.z - (floor + 0.09 + SHIN), 0.3), -0.32)         # (thighs down to a high seat, up from a low one)
            knee = hip + way * math.sqrt(THIGH * THIGH - rise * rise) - Z * rise + left * (spread - 0.1) * sy
            drop = knee.z - (floor + 0.09)
            ankle = knee - Z * min(drop, SHIN) + way * math.sqrt(max(SHIN * SHIN - drop * drop, 0.0))
            if inside:
                ankle = inside(ankle)
            foot = way
        else:                                            # ("kneel": knees on the floor ahead, shins back along it)
            hip = at(-0.03, 0.1 * sy, -HIP_SIT)
            drop = max(min(hip.z - (floor + 0.07), THIGH * 0.98), -THIGH * 0.5)
            knee = hip + way * math.sqrt(THIGH * THIGH - drop * drop) - Z * drop + left * (spread - 0.1) * sy
            ankle = knee - way * SHIN
            ankle.z = floor + 0.06
            foot = -way
        if inside and pose != "kneel":
            knee = inside(knee)                          # (a knee that would bend out through her side comes in)
        m.rod(hip, knee, 0.08, "suit", 0.064, sides=8 if m.fine else 6)                # his thigh
        m.ball(knee, 0.066, "suit", u, v)
        m.rod(knee, ankle, 0.06, "suit", 0.05, sides=8 if m.fine else 6)               # his shin
        if m.fine:
            _boot(m, ankle, foot, flat=pose == "kneel", inside=inside)
        knees.append(knee)
    return knees


def deck(m, hull, x_hi, x_lo, z, paint="inside", room=0.015):
    """A floor laid flat across her at height z, from x_hi back to x_lo, as wide as her skin lets it be there: no
    plank stands out through her glass or her sides."""
    n = 10 if m.fine else 4
    while x_hi > x_lo + 0.1 and z < bottom_at(hull, x_hi) + room + 0.01:
        x_hi -= 0.02                                     # (it ends where her skin comes up to it, at either end)
    while x_lo < x_hi - 0.1 and z < bottom_at(hull, x_lo) + room + 0.01:
        x_lo += 0.02
    left, right = [], []
    for k in range(n + 1):
        x = x_hi - (x_hi - x_lo) * k / n
        half = room_at(hull, Vector((x, 0.0, z)), room) if z > bottom_at(hull, x) + room else 0.0
        half = half or 0.0
        left.append(Vector((x, half, z)))
        right.append(Vector((x, -half, z)))
    verts = left + right
    faces = [(k, k + 1, n + 2 + k, n + 1 + k) for k in range(n)]
    m.add(verts, faces, paint, False, lambda c: Z)


def bombsight(m, at, floor):
    """The bombsight on its stand in front of him, between his knees, as he bends over it with his eye to it: the
    eyepiece's rubber cup just ahead of his right eye on its tube, the sight below that with its window ahead and a
    knob each side where his hands are, the box of its stabiliser under the sight, and its stand down to her floor.
    Returns his two grips (his left first)."""
    way, left = _ahead(at)
    yaw = math.degrees(math.atan2(way.y, way.x))
    gaze = (at(1.0, 0.0, 0.0) - at(0.0, 0.0, 0.0)).normalized()
    cup = at(0.09, -0.03, 0.01) + gaze * 0.055                                                        # (on his right eye's line of sight)
    sight = at(0.1, 0.0, -0.07) + way * 0.06
    sight.z = cup.z - 0.18                                                                            # (its top just under the eyepiece)
    m.rod(cup, cup - Z * 0.06, 0.03, "leather", sides=8)                                              # (the eyepiece's rubber cup)
    m.rod(cup - Z * 0.06, Vector((cup.x, cup.y, sight.z + 0.08)), 0.018, "gunmetal", sides=6)       # (its short tube down into the sight)
    m.box(sight, (0.24, 0.13, 0.18), "gunmetal", yaw=yaw)
    m.rod(sight + way * 0.12, sight + way * 0.135, 0.032, "lens", sides=8)                            # (its window ahead)
    base = sight + way * 0.03 - Z * 0.16
    m.box(base, (0.3, 0.2, 0.12), "gunmetal", yaw=yaw)
    grips = []
    for sy in (1.0, -1.0):
        m.rod(sight + left * 0.065 * sy, sight + left * 0.1 * sy, 0.024, "gunmetal", sides=8)             # (a knob)
        grips.append(sight + left * 0.125 * sy)
    if base.z - 0.06 - floor > 0.04:
        m.rod(Vector((base.x, base.y, floor)), base - Z * 0.06, 0.035, "gunmetal", sides=8)          # (its stand)
    return grips


def lap(m, at):
    """A man's seat under him, and his thighs on it, his knees up and his shins down to the floor ahead of him."""
    m.box(at(0.06, 0.0, -0.8), (0.42, 0.42, 0.05), "inside")                                                      # his seat
    trunk(m, at)                                         # (his belly and hips, down to his seat)
    u, v = (8, 5) if m.fine else (6, 3)
    for sy in (1.0, -1.0):
        hip, knee = at(-0.02, 0.11 * sy, -0.72), at(0.38, 0.13 * sy, -0.62)
        m.rod(hip, knee, 0.075, "suit", 0.062, sides=7)                                                          # his thighs
        m.ball(knee, 0.064, "suit", u, v)
        m.rod(knee, at(0.62, 0.14 * sy, -0.98), 0.058, "suit", 0.05, sides=7)                                     # his shins, to the rudder pedals


def wheel(m, at, floor):
    """A bomber's pilot at his wheel: his seat on its frame, his legs, his boots on the rudder pedals, and both his
    hands on the wheel at the head of his control column."""
    lap(m, at)
    seat = at(0.06, 0.0, -0.825)
    if seat.z - floor > 0.08:
        m.box((seat.x, seat.y, 0.5 * (seat.z + floor)), (0.3, 0.3, seat.z - floor), "inside")
    hub = at(0.4, 0.0, -0.38)
    m.rod(Vector((hub.x + 0.12, hub.y, floor)), hub, 0.02, "gunmetal", sides=6)
    grips = [at(0.38, 0.15 * sy, -0.36) for sy in (1.0, -1.0)]
    m.rod(grips[0], grips[1], 0.014, "gunmetal", sides=6)
    for g in grips:
        m.rod(g - Z * 0.02, g + Z * 0.07, 0.016, "leather", sides=6)
    arms(m, at, [g + Z * 0.03 for g in grips])
    # (his boots at the ends of his shins, on the rudder pedals, each on a post down to her floor: where he sits in her
    #  glass nose, the whole of him is seen)
    way, _ = _ahead(at)
    for sy in (1.0, -1.0):
        ankle = at(0.62, 0.14 * sy, -0.98)
        _boot(m, ankle, way)
        pedal = ankle + way * 0.21 - Z * 0.02
        m.box(pedal, (0.03, 0.1, 0.15), "gunmetal", yaw=math.degrees(math.atan2(way.y, way.x)))
        if pedal.z - 0.075 - floor > 0.04:
            m.rod(Vector((pedal.x, pedal.y, floor)), pedal - Z * 0.075, 0.015, "gunmetal", sides=5)


def _aim(hull, s, x, y, high):
    """A man in her glass nose as the near model has him: (how far he leans forward, how far his hips are behind his
    head, her floor under him or None, his head's height, whether he lies on his front). Her pilot (high None) sits up
    under her glass on his own floor. Her bomb-aimer sits on a low seat and bends over his sight with his eye to it:
    her floor as low as her skin lets it lie under his seat and under his feet, his seat a quarter of a metre above it,
    and his head where that puts it, if there is room for it under her glass. Her nose gunner sits upright at his gun.
    Where her nose is too low to sit in, either lies on his front, as the He 111's men did."""
    if high is None:
        return 0.0, 0.0, None, body_top(hull, x, -y) - 0.2, False
    lean = 0.0 if s.get("nose_gunner") else AIM_LEAN
    back = HIP_SIT * math.sin(math.radians(lean))
    zf = max(bottom_at(hull, q, -y + d) for q in (x - back, x - back + 0.45) for d in (-0.19, 0.19)) + 0.05
    z = hull.z(-x) + high
    if lean:
        z = min(zf + 0.25 + 0.08 + HIP_SIT * math.cos(math.radians(lean)), body_top(hull, x, -y) - 0.21)
    if z - bottom_at(hull, x, -y) < 0.95:
        return 0.0, 0.0, None, z, True
    return lean, back, zf, z, False


def glazed_nose(m, f, s, G, hull):
    """A bomber's glass nose: a bulkhead behind it and a floor in it, so that nobody looks through her, and the men seen
    in it. The far model has each man to his chest over a plain floor; the near one has the whole of each of them."""
    glazed = s.get("glazed")
    if not glazed:
        return
    m.use("plane")
    crew = []
    for x, y, high, face in NOSE_CREW.get(f.name, []):
        if m.fine and high is not None and not s.get("nose_gunner"):
            x = AIM_AT.get(f.name, x)
        crew.append((x, y, high, face, _aim(hull, s, x, y, high) if m.fine else (0.0, 0.0, None, None, False)))
    sat = [(x, c) for x, y, high, face, c in crew if c[2] is not None]          # (near: the men who sit on her floor)
    pilots = [c[3] for x, y, high, face, c in crew if high is None]
    xb = glazed[1] - 0.03
    wall_paint = "inside"
    bands = s.get("glass_bands", [])

    def window_at(x, high):
        sh = math.copysign(abs(math.sin(math.radians(high))) ** hull.power, high)
        for x0, x1, half, *hi in bands:
            up = hi[0] if hi else 0.3
            if x1 - 0.03 < x < x0 + 0.03 and up - half / hull.h(-x) - 0.03 < sh < up + half / hull.h(-x) + 0.03:
                return True
        return False
    if f.name in NOSE_ROOM and m.fine:
        xb, wall_paint = NOSE_ROOM[f.name], "room"
        liner(m, f, hull, glazed[1], xb, window_at, inset=0.03, paint="room")
    elif sat:
        # (his seat and his hips stand ahead of the wall behind her glass: where they would reach it, the wall goes back
        #  to the first of her rings behind them, and her skin is lined dark from there forward, as the B-17's room is)
        need = min(x - c[1] - 0.26 for x, c in sat)
        if need < xb:
            rings = [r for r in f.rings_x if r <= need]
            xb = max(rings) if rings else need
            liner(m, f, hull, glazed[1], xb, window_at, inset=0.03, paint=wall_paint)
    m.disc((xb, 0.0, hull.z(-xb)), X, hull.h(-xb) * 0.97, wall_paint, 24 if m.fine else 12, oval=hull.w(-xb) / hull.h(-xb))
    x1 = hull.front - 0.45
    floor = hull.z(-0.5 * (x1 + glazed[1])) - 0.5 * hull.h(-0.5 * (x1 + glazed[1])) if x1 > glazed[1] + 0.1 else None
    step = None                                          # (near: where her pilots' floor ends ahead, over her aimer's)
    if x1 > glazed[1] + 0.1 and (not sat or s.get("nose_wheel")):
        xm = 0.5 * (x1 + glazed[1])
        m.box((xm, 0.0, hull.z(-xm) - 0.5 * hull.h(-xm)), (x1 - glazed[1], 1.3 * hull.w(-x1), 0.04), wall_paint)
    elif sat and pilots and floor is not None:
        # (her pilots sit up under her glass behind her aimer: their own floor, a seated man's height under their heads,
        #  from the wall forward to behind his seat, and a step down from it to his)
        floor = max(floor, min(pilots) - 1.25)
        step = min(x1, min(x - c[1] - 0.26 for x, c in sat))
        deck(m, hull, step, xb, floor, wall_paint)
        low = min(c[2] for x, c in sat)
        ht = room_at(hull, Vector((step, 0.0, floor)), 0.015) or 0.0
        hb = room_at(hull, Vector((step, 0.0, low)), 0.015) or 0.0
        if ht > 0.0 and hb > 0.0:
            m.add([Vector((step, ht, floor)), Vector((step, -ht, floor)), Vector((step, -hb, low)), Vector((step, hb, low))],
                  [(0, 1, 2, 3)], wall_paint, False, lambda c: X)
    for x, y, high, face, (lean, back, zf, z, prone) in crew:
        if not m.fine:
            z = body_top(hull, x, -y) - 0.2 if high is None else hull.z(-x) + high
        at = figure(m, (x, y, z), face, vest=False, lean=lean, chest=not prone)
        if not m.fine:
            continue
        if high is None:
            m.box(at(-0.2, 0, -0.3), (0.05, 0.42, 0.36), "inside")
            if floor is not None and (s.get("nose_wheel") or step is not None):
                wheel(m, at, floor)                        # (her pilot flies her from inside her glass nose)
        elif prone:
            if s.get("nose_wheel") and x1 > glazed[1] + 0.1:
                lie(m, hull, at, floor + 0.02, deck_too=False)   # (on her floor)
            else:
                lie(m, hull, at, z - 0.42)
            hold_gun(m, f, s, hull, Vector((x, y, z)), face, at, legs=False)
        else:
            deck(m, hull, x - back + 0.66, step if step is not None else xb if not s.get("nose_wheel") else x - back - 0.5, zf, wall_paint)
            trunk(m, at)
            lvl = upright(at, face)
            m.box(lvl(-0.02, 0.0, -0.8), (0.36, 0.38, 0.05), "inside")                                  # (his seat)
            seat = lvl(-0.02, 0.0, -0.825)
            if seat.z - zf > 0.06:
                m.rod(Vector((seat.x, seat.y, zf)), seat, 0.03, "gunmetal", sides=6)                   # (its post)
            if s.get("nose_gunner"):
                legs(m, lvl, "sit", zf, inside=lambda p: _in(hull, p, 0.07))
                hold_gun(m, f, s, hull, Vector((x, y, z)), face, at, legs=False)     # (the man at her nose gun: both his hands on it)
            else:
                legs(m, lvl, "sit", zf, spread=0.2, inside=lambda p: _in(hull, p, 0.07))
                arms(m, at, bombsight(m, at, zf), inside=lambda el, sh: tuck(hull, el, sh, 0.09))   # (both his hands on its knobs)
    m.use("plane")


def lie(m, hull, at, floor, deck_too=True):
    """A man lying on his front on a pad on her floor, up on his elbows with his head raised to look ahead: his chest,
    belly and hips along the floor behind his head, his legs straight back, his toes down."""
    u, v = (10, 6) if m.fine else (6, 4)
    way, left = _ahead(at)
    if deck_too:                                         # (his own floor, where she has none there already)
        deck(m, hull, at(0.3, 0, 0).x, at(-1.95, 0, 0).x, floor)
    pad = at(-0.95, 0.0, 0.0)
    m.box((pad.x, pad.y, floor + 0.025), (1.7, 0.5, 0.05), "leather", yaw=math.degrees(math.atan2(way.y, way.x)))    # (his pad)
    top = floor + 0.05
    m.ball(at(-0.3, 0.0, 0.0) + Z * (top + 0.14 - at(0, 0, 0).z), _r(at, 0.27, 0.21, 0.14), "suit", u, v)      # (his chest)
    m.ball(at(-0.62, 0.0, 0.0) + Z * (top + 0.12 - at(0, 0, 0).z), _r(at, 0.2, 0.19, 0.12), "suit", u, v)      # (his belly)
    m.ball(at(-0.85, 0.0, 0.0) + Z * (top + 0.12 - at(0, 0, 0).z), _r(at, 0.15, 0.19, 0.12), "suit", u, v)     # (his hips)
    for sy in (1.0, -1.0):
        hip = at(-0.87, 0.1 * sy, 0.0)
        hip.z = top + 0.1
        knee = hip - way * THIGH + left * 0.02 * sy
        knee.z = top + 0.07
        ankle = knee - way * SHIN + left * 0.01 * sy
        ankle.z = top + 0.07
        m.rod(hip, knee, 0.08, "suit", 0.064, sides=8)
        m.ball(knee, 0.064, "suit", 8, 5)
        m.rod(knee, ankle, 0.06, "suit", 0.05, sides=8)
        m.rod(ankle + way * 0.03, ankle - way * 0.05 - Z * 0.04, 0.062, "boot", 0.05, sides=7)                 # (his boot, toe down)
    m.rod(at(-0.02, 0.0, -0.25), Vector((at(-0.3, 0, 0).x, at(-0.3, 0, 0).y, top + 0.16)), 0.13, "suit", 0.15, sides=10)   # (shoulders down to his chest)


def _in(hull, p, room):
    """A boot's place brought in toward her middle line and up off her skin until it is `room` inside her."""
    q = Vector(p)
    q.z = max(q.z, bottom_at(hull, q.x, q.y) + room)
    lim = room_at(hull, q, room)
    if lim is not None and abs(q.y) > lim:
        q.y = math.copysign(lim, q.y)
    return q


def gunners(m, f):
    """In each turret: the gunner, his head up under the dome between his two guns and a hand on the grip of each; the
    guns' breeches beside his head; and the dark of the hole he stands in."""
    for k, c, radii in f.turrets:
        if k in f.balls or k in f.remote:
            continue                                      # (see ball_turret; a chin turret is aimed from the nose above it, and a man in it hung out below)
        guns = f.guns_of.get(k, [])
        way = sum(((b - a) for a, b in guns), Vector())
        way.z = 0.0
        face = math.degrees(math.atan2(way.y, way.x)) if way.length > 1e-6 else 0.0
        way = way.normalized() if way.length > 1e-6 else X.copy()
        m.use(f"turret.{k}")
        lift = max(0.1 * radii[2], min(0.52 * radii[2], radii[2] - 0.16))      # (his head clear under the dome, not cut off by her roof)
        back = 0.22 if len(guns) == 1 else 0.1                                 # (one gun is on his middle line, under his eye: he sits back from its breech)
        at = figure(m, c + Z * lift - way * back, face, vest=False)
        m.disc(c + Z * 0.012, Z, 0.9 * min(radii[0], radii[1]), "inside", 20 if m.fine else 10)
        grips = []
        if m.fine:
            m.use(f"turret.{k}.guns")
            for a, b in guns:
                d = (b - a).normalized()
                over = (Z - d * Z.dot(d)).normalized()                   # (square up off the gun)
                m.rod(a - d * 0.1, a + d * 0.24, 0.05, "gunmetal", sides=8)                       # (its breech, ahead of his hands)
                grip = a - d * 0.15
                for sz in (-1.0, 1.0):                                                           # (and its spade grip behind that)
                    m.rod(a - d * 0.1 + over * 0.035 * sz, grip + over * 0.035 * sz, 0.011, "gunmetal", sides=5)
                if len(guns) == 1:
                    # (one gun: its spade grips stand out each side of its breech, a hand on each)
                    side = d.cross(over).normalized()
                    pair = [grip + side * 0.085 * sy for sy in (-1.0, 1.0)]
                    m.rod(pair[0], pair[1], 0.011, "gunmetal", sides=5)
                    for g in pair:
                        m.rod(g - over * 0.05, g + over * 0.05, 0.017, "gunmetal", sides=6)
                    grips += pair
                    continue
                m.rod(grip - over * 0.05, grip + over * 0.05, 0.017, "gunmetal", sides=6)
                grips.append(grip)
        if len(grips) == 4:
            # (four guns, a pair each side of him one over the other: a handle each side from the upper gun's grip
            #  down to the lower's, and a hand on the middle of each)
            left = at(0.0, 1.0, 0.0) - at(0.0, 0.0, 0.0)
            grips.sort(key=lambda g: -(g - c).dot(left))
            m.use(f"turret.{k}.guns")
            for pair in (grips[:2], grips[2:]):
                m.rod(pair[0], pair[1], 0.012, "gunmetal", sides=5)
            grips = [0.5 * (grips[0] + grips[1]), 0.5 * (grips[2] + grips[3])]
        if len(grips) == 2:
            left = at(0.0, 1.0, 0.0) - at(0.0, 0.0, 0.0)
            grips.sort(key=lambda g: -(g - c).dot(left))
            m.use(f"turret.{k}")
            arms(m, at, grips, fore=f"turret.{k}.guns", inside=lambda el, sh: tuck(f.hull, el, sh, 0.09))
    m.use("plane")


def flex_pair(s, base):
    """How far each gun of a pair on one free mounting stands out from its middle; 0 for a single gun. (A pair is one
    rod in her numbers, off her middle line: planes.py mirrors it, and makes the two one mounting.)"""
    if abs(base.y) > 1e-6:
        return 0.0
    for kind, v in s.get("extras", []):
        if kind == "rod" and v[4] == "steel" and abs(v[0][0] - base.x) < 0.01 and abs(v[0][2] - base.z) < 0.01 and 0.0 < abs(v[0][1]) < 0.3:
            return abs(v[0][1])
    return 0.0


def rear_gunner(m, f, s, G, hull):
    """A man who sits facing aft under her hood with a gun on a free mounting in front of him (the rear gunner of a
    dive bomber or a torpedo bomber, the gunner on a bomber's back): see hold_gun."""
    if not m.fine or not f.flex:
        return
    for head, face, at in getattr(f, "seated", []):
        if abs(abs(face) - 180.0) > 1.0:
            continue
        hold_gun(m, f, s, hull, head, face, at)
    m.use("plane")


def hold_gun(m, f, s, hull, head, face, at, legs=True):
    """A man at a gun on a free mounting in front of him: each gun's breech on its own line back to a pair of grips
    where his hands fall as he sits, the post it swings on, his seat and his legs (`legs`); and both his hands on the
    grips (Brian, 2026-11-19: every gunner with his two hands on his guns). His forearms and hands go with the gun.
    False if no gun of hers is in front of him: he keeps watch."""
    if not m.fine or not f.flex:
        return False
    taken = f.__dict__.setdefault("guns_held", set())
    ahead = Vector((math.cos(math.radians(face)), math.sin(math.radians(face)), 0.0))
    near = [(nm, b, t) for nm, b, t in f.flex if nm not in taken and (b - head).dot(ahead) > -0.1 and (b - head).length < 1.4 and abs(b.y - head.y) < 0.45]
    if not near:
        return False
    nm, base, tip = min(near, key=lambda g: (g[1] - head).length)
    taken.add(nm)
    d = (tip - base).normalized()
    want = at(0.33, 0.0, -0.2)                        # (where his hands fall as he sits: a forearm ahead of his chest)
    hold = base - d * min(max((base - want).dot(d), 0.2), 0.8)      # (the middle of his grips, on the line of his guns)
    across = d.cross(Z)
    across = across.normalized() if across.length > 1e-6 else Y.copy()
    over = across.cross(d).normalized()
    if over.z < 0.0:
        over = -over
    half = flex_pair(s, base)
    m.use(nm)
    for sy in ((-1.0, 1.0) if half else (0.0,)):
        off = across * half * sy
        m.rod(hold + off + d * 0.03, base + off + d * 0.1, 0.032, "gunmetal", sides=8)       # (its breech, back from where its barrel begins)
        m.box(hold + off + d * 0.2 - over * 0.075, (0.1, 0.05, 0.075), "gunmetal")            # (the box its rounds feed from)
    grips = [hold + across * (half + 0.08) * sy for sy in (-1.0, 1.0)]
    m.rod(grips[0], grips[1], 0.012, "gunmetal", sides=5)                                      # (the bar across the back of the breech)
    for g in grips:
        m.rod(g - over * 0.06, g + over * 0.06, 0.017, "gunmetal", sides=6)                    # (a grip for each hand)
    m.use("plane")
    deck = body_top(hull, base.x, -base.y)
    if 0.05 < base.z - deck < 0.45:
        m.rod(base - Z * 0.03, Vector((base.x, base.y, deck - 0.03)), 0.022, "gunmetal", sides=6)   # (the post it swings on)
    if legs:
        lap(m, at)
    left = at(0.0, 1.0, 0.0) - at(0.0, 0.0, 0.0)
    grips.sort(key=lambda g: -(g - hold).dot(left))
    arms(m, at, grips, fore=nm, inside=lambda el, sh: tuck(hull, el, sh, 0.09))
    m.use("plane")
    return True


def room_at(hull, p, room):
    """How far out from her middle line a thing may stand at this place and stay `room` inside her skin; None above
    her top (there it is under her glass)."""
    w, h, zc = hull.w(-p.x), max(hull.h(-p.x), 1e-6), hull.z(-p.x)
    t = abs(p.z - zc) / h
    if p.z > zc and t >= 1.0:
        return None
    e = hull.square_at(p.x) if hasattr(hull, "square_at") else 2.0 / hull.power
    return max(w * (1.0 - min(t, 1.0) ** e) ** (1.0 / e) - room, 0.04)


def tuck(hull, elbow, shoulder, room):
    """An elbow brought in towards her middle line until the whole upper arm, from the shoulder down to it, is inside
    her skin: a man who sits high in a narrow fuselage holds his elbows in."""
    e = Vector(elbow)
    hand = 1.0 if shoulder.y >= 0.0 else -1.0
    for t in (1.0, 0.75, 0.5, 0.3):
        p = shoulder + (e - shoulder) * t
        lim = room_at(hull, p, room)
        if lim is not None and abs(p.y) > lim:
            e.y = shoulder.y + (hand * lim - shoulder.y) / t
            if hand * e.y < 0.04:
                e.y = hand * 0.04
    return e


# The machines whose belly turret is a steel ball.
BALL = {"B17"}


def ball_turret(m, f):
    """A belly turret that is a steel ball in her belly's paint, hung in a yoke that trains. The ball itself lifts with
    its two guns, and has one round window between them, through which its gunner, curled up inside, is seen. (The
    art director, 2026-11-18: a clear sphere with a man sitting upright in it reads as a goldfish bowl.)"""
    for k, c, radii in f.turrets:
        if k not in f.balls:
            continue
        guns = f.guns_of.get(k, [])
        d = sum(((b - a) for a, b in guns), Vector())
        d = d.normalized() if d.length > 1e-6 else X.copy()
        side = Z.cross(d).normalized()                    # (to port; the line the ball lifts about)
        dw = (d + d.cross(side) * 0.16).normalized()      # (his window looks a little above his guns: it is at his eye)
        up = dw.cross(side)
        R = min(radii) * 0.985
        around = 28 if m.fine else 12
        steps = 12 if m.fine else 5
        tw = math.asin(0.165 / R)                         # (the half angle of his window)

        def on(t, a, r=R):
            """A place on the ball: t from the middle of his window, a round it."""
            return c + (dw * math.cos(t) + (side * math.cos(a) + up * math.sin(a)) * math.sin(t)) * r

        m.use(f"turret.{k}.guns")
        for r, paint in ((R, "under"), (R - 0.025, "inside")):                          # (the ball, and the dark inside it)
            m.lathe(c, dw, [(r * math.cos(t), r * math.sin(t)) for t in (tw + (math.pi - tw) * i / steps for i in range(steps + 1))], paint, around, caps=(False, False))
        m.lathe(c, dw, [((R - 0.006) * math.cos(t), (R - 0.006) * math.sin(t)) for t in (tw * i / 3.0 for i in range(4))], "glass", around, caps=(False, False))
        ring = [2.0 * math.pi * i / around for i in range(around + 1)]
        m.strip([on(tw, a) for a in ring], [(on(tw, a) - c).normalized() for a in ring], 0.02, "gunmetal", lift=0.006)
        for a, b in guns:                                 # (a collar where each gun comes out of the ball)
            g = (b - a).normalized()
            along = (c - a).dot(g)
            off = (a + g * along - c).length
            if off < R:
                p = a + g * (along + math.sqrt(R * R - off * off))
                m.rod(p - g * 0.05, p + g * 0.07, 0.05, "gunmetal", sides=10)
        # his head at the window, his body and his knees behind it
        u, v = (10, 6) if m.fine else (6, 4)
        head = c + dw * (R - 0.27)
        if m.fine:                                        # (close: his face under his helmet, as every airman's: crewface.py)
            seat = CREW["n"]
            CREW["n"] += 1
            goggles, masked = look_of(CREW["nation"], CREW["plane"], seat)
            crewface.head(m, lambda dx, dy, dz: head + dw * dx + side * dy + up * dz, CREW["nation"], CREW["plane"], seat,
                          goggles=goggles, mask=masked, chest=c - dw * 0.08 + up * 0.02 + (dw * 0.6 - up * 0.8 - side * 0.2).normalized() * 0.29)
        else:
            m.ball(head, 0.1, "leather", u, v)
            m.ball(head + dw * 0.05 - up * 0.012, 0.076, "skin", u, v)
        m.ball(c - dw * 0.08 + up * 0.02, 0.3, "suit", u, v)
        for hand in (-1.0, 1.0):
            m.ball(c + dw * 0.17 + side * 0.25 * hand + up * 0.1, 0.1, "suit", u, v)
            if m.fine:
                grip = head + dw * 0.17 + side * 0.105 * hand + up * 0.1      # (a hand on the grip of each gun, up beside his face at the window)
                m.rod(c + dw * 0.17 + side * 0.25 * hand + up * 0.1, grip, 0.045, "suit", 0.038, sides=6)
                m.ball(grip, 0.045, "leather", 8, 5)
        if m.fine:
            m.rod(head + dw * 0.17 - side * 0.16 + up * 0.1, head + dw * 0.17 + side * 0.16 + up * 0.1, 0.012, "gunmetal", sides=5)
        if m.fine:
            # the seams of the ball's two halves, its door behind him, a ring of rivets round his window
            seam = [tw + 0.09 + (2.0 * math.pi - 2.0 * tw - 0.18) * i / 40.0 for i in range(41)]
            pts = [c + (dw * math.cos(t) + up * math.sin(t)) * R for t in seam]
            m.strip(pts, [(q - c).normalized() for q in pts], 0.005, "line", lift=0.004)
            pts = [c + (side * math.cos(a) + up * math.sin(a)) * R for a in ring]
            m.strip(pts, [(q - c).normalized() for q in pts], 0.005, "line", lift=0.004)
            door = [on(math.radians(t), math.radians(a)) for t, a in ((128, 55), (128, 125), (168, 125), (168, 55), (128, 55))]
            door = [door[i] + (door[i + 1] - door[i]) * j / 6.0 for i in range(4) for j in range(6)] + [door[0]]
            door = [c + (q - c).normalized() * R for q in door]
            m.strip(door, [(q - c).normalized() for q in door], 0.005, "line", lift=0.004)
            for i in range(16):
                q = on(tw + 0.085, 2.0 * math.pi * (i + 0.5) / 16.0)
                m.disc(q + (q - c).normalized() * 0.004, q - c, 0.009, "rivet_under", 4)
        # the yoke it hangs in: a boss at each end of the line it lifts about, an arm from each up into her belly
        m.use(f"turret.{k}")
        for hand in (-1.0, 1.0):
            boss = c + side * (R - 0.02) * hand
            m.rod(boss, boss + side * 0.075 * hand, 0.12, "gunmetal", sides=14 if m.fine else 8)
            m.rod(boss + side * 0.045 * hand, c + side * (R * 0.86) * hand + Z * 0.5, 0.05, "under", 0.06, sides=8)
    m.use("plane")


def exhausts(m, f, s, G, hull):
    """Her exhausts: a row of stubs along each side of an engine in line, a pipe each side of a radial's cowling."""
    sides = 8 if m.fine else 5
    f.stains = []                                         # (a body, where along it the stain starts, how far it runs, where round the body): see planes.py's weather

    def stub(p, n, part, long=0.13, r=0.046, out=0.075, down=None, turn=0.0):
        drop = 0.01 if down is None else long * math.tan(math.radians(down))
        tip = p + n * out + Vector((-long, 0.0, -drop))
        m.use(part)
        m.rod(port(p - n * 0.02), port(tip), r, "exhaust", r * 0.88, sides=sides)
        way = (tip - (p - n * 0.02)).normalized()
        if turn:
            # (its last part turned out from her side: an elbow, and a short length that its mouth is at the end of)
            side = Vector((0.0, 1.0 if n.y > 0.0 else -1.0, 0.0))
            bend = (way * math.cos(math.radians(turn)) + side * math.sin(math.radians(turn))).normalized()
            m.ball(port(tip), (r * 0.88, r * 0.88, r * 0.88), "exhaust", sides, 6 if m.fine else 4)
            m.rod(port(tip), port(tip + bend * r), r * 0.88, "exhaust", sides=sides)
            tip, way = tip + bend * r, bend
        m.disc(port(tip + way * 0.002), port(way), r * 0.66, "engine", sides)
        return tip

    def round_to(body, x, a, z):
        """How far round her, from `a` on the same side, her skin at x stands at height z: where the stain of a pipe
        whose mouth is lower (or higher) than its root begins."""
        up = 1.0 if z > body.at(x, a).z else -1.0
        step = math.radians(0.5) * up * (1.0 if math.cos(a) >= 0.0 else -1.0)
        b = a
        for k in range(1, 181):
            if abs(math.sin(a + step * k)) > 0.999 or (body.at(x, a + step * k).z - z) * up >= 0.0:
                return a + step * k
            b = a + step * k
        return b

    def row(body, x0, x1, angle, part):
        for hand in (1.0, -1.0):
            a = math.radians(angle) if hand > 0.0 else math.pi - math.radians(angle)
            for k in range(6):
                x = x0 + (x1 - x0) * k / 5.0
                stub(body.at(x, a), body.out(x, a), part)
            f.stains.append((body, min(x0, x1) - 0.05, 2.6, a))

    def pipes(body, x, part, long=0.4, angle=-28.0, out=0.075, r=0.05, down=None, turn=0.0, only=None):
        for a in (math.radians(angle), math.radians(180.0 - angle)):
            if only is not None and (math.cos(a) > 0.0) != (only > 0.0):
                continue                                   # (a pipe on one side of her only: `exhaust_side`)
            mouth = stub(body.at(x, a), body.out(x, a), part, long, r, out, down, turn)
            if down is not None or turn:
                a = round_to(body, x - long, a, mouth.z)       # (a pipe run down or turned out: its stain from its mouth's height)
            f.stains.append((body, x - long, 2.4, a))

    if not s.get("engines") or s.get("nose_engine"):                                 # (a trimotor's nose engine too)
        if s.get("stubs"):
            row(hull, s["stubs"][0], s["stubs"][1], s["stubs"][2], "plane")
        elif s.get("engine") == "radial" and s.get("exhaust"):
            # (`exhaust`: her own pipe each side, (where along her its root is, degrees round her from her beam, how long,
            #  how far its mouth stands off her skin, its radius, degrees it runs down, degrees its mouth is turned out))
            x, angle, long, out, r, down, turn = tuple(s["exhaust"]) + (-28.0, 0.4, 0.075, 0.05, None, 0.0)[len(s["exhaust"]) - 1:]
            # (`exhaust_side`: her one pipe on one side only, +1 her starboard, -1 her port: the Devastator's, to starboard)
            pipes(hull, x, "plane", long, angle, out, r, down, turn, s.get("exhaust_side"))
        elif s.get("engine") == "radial" and s.get("nose_back") is not None:
            pipes(hull, s["nose_back"] - 0.02, "plane")
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            part = ("wing.starboard" if hand > 0.0 else "wing.port") if out > f.y_cut else "plane"
            side = out * hand
            nacelle = nacelle_of(G, lead, z, is_radial, back, side, s)
            if is_radial:
                pipes(nacelle, lead + 0.7, part, 0.5)
            else:
                row(nacelle, lead + 1.75, lead + 0.95, 10.0, part)


def guns(m, f, s, G, hull):
    """Her fixed guns: a muzzle on every barrel that stands out of the wing, a port where one does not (doped over in red
    on a British fighter, as they were until the guns had fired), and the guns over the engine in their troughs."""
    uk = nation(s) == "UK"
    for (out, back, length, bore) in s.get("guns", []):
        for hand in (-1.0, 1.0):
            W = G["Wing"](s["wing"], hand)
            at = W.at(out, 0.02, True)
            muzzle = port((at.x + length, at.y, at.z - 0.03))
            m.use(("wing.port" if hand < 0.0 else "wing.starboard") if out > f.y_cut else "plane")
            if length < 0.12:
                # (a port in the leading edge: on a British fighter a patch of red doped fabric is stuck over it)
                if uk:
                    cs = [0.04, 0.02, 0.007, 0.0]
                    pts = [W.at(out, c, True) for c in cs] + [W.at(out, c, False) for c in reversed(cs[:-1])]
                    mid = 0.5 * (W.at(out, 0.07, True) + W.at(out, 0.07, False))
                    m.strip([port(q) for q in pts], [port((q - mid).normalized()) for q in pts], 0.05, "patch", lift=0.005)
                else:
                    m.disc(port(W.at(out, 0.0, True)) + X * 0.004, X, max(bore * 1.4, 0.035), "engine", 10)
            else:
                m.rod(muzzle - X * 0.09, muzzle + X * 0.012, bore * 1.55, "gunmetal", sides=8)
                m.disc(muzzle + X * 0.014, X, bore * 0.8, "engine", 8)
    if f.name in NOSE_GUNS or s.get("nose_guns"):
        # (`nose_guns`: her own troughs, (degrees round from her top, how far the barrel stands out of its trough))
        own = s.get("nose_guns") or ()
        ang, lift = ((own,) if isinstance(own, (int, float)) else tuple(own)) + (17.0, 0.028)[1 if isinstance(own, (int, float)) else len(own):]
        hub_x = f.hubs[0].x
        m.use("plane")
        for sy in (1.0, -1.0):
            a = math.radians(90.0 + ang * sy)
            xs = [hub_x - 0.5 - 0.85 * k / 6.0 for k in range(7)]
            pts, fac = [port(hull.at(x, a)) for x in xs], [port(hull.out(x, a)) for x in xs]
            m.strip(pts, fac, 0.04, "engine", lift=0.006)
            m.rod(pts[-1] + fac[-1] * lift, pts[1] + fac[1] * lift, 0.02, "gunmetal", sides=6)
    if s.get("cowl_gun"):
        # (`cowl_gun`: one fixed gun firing through the ring of her cowling, (x, y, z) of its port in the game's axes, the
        #  Devastator's on her starboard side: the short blast tube standing out of the ring, and its dark bore)
        p = Vector(s["cowl_gun"])
        m.use("plane")
        m.rod(port(p - X * 0.07), port(p + X * 0.006), 0.03, "gunmetal", sides=10 if m.fine else 6)
        m.disc(port(p + X * 0.008), X, 0.017, "engine", 10 if m.fine else 6)


def fasteners(m, f, s, G, hull):
    """The cowling of a single-engined machine: a row of fasteners round its after edge and along each side, as dots."""
    if s.get("engines") or s.get("jets") or not m.fine:
        return
    long = hull.front - hull.back
    back = s.get("nose_back") if s.get("nose_back") is not None else hull.front - long * 0.2
    front = f.hubs[0].x - 0.35
    m.use("plane")

    def dot(x, a):
        p, n = port(hull.at(x, a)), port(hull.out(x, a))
        m.disc(p + n * 0.006, n, 0.011, "line", 6)
    around = max(12, int(2.0 * math.pi * hull.h(-back) / 0.13))
    for k in range(around):
        dot(back + 0.035, 2.0 * math.pi * (k + 0.5) / around)
    steps = max(3, int((front - back) / 0.14))
    for a in (math.radians(12.0), math.radians(168.0), math.radians(-35.0), math.radians(215.0)):
        for k in range(1, steps):
            dot(back + (front - back) * k / steps, a)


def lamps(m, f, s, G, hull):
    """Her navigation lamps: red to port, green to starboard, white astern. (`lamps` in her numbers, (x, y out): each
    wingtip lamp there, on the tip itself and standing out of it, as the Wildcat's are.)"""
    for hand, nm, paint in ((-1.0, "port", "lamp_red"), (1.0, "starboard", "lamp_green")):
        W = G["Wing"](s["wing"], hand)
        u = W.tip - max(0.07, 0.012 * W.tip)
        p = 0.5 * (W.at(u, 0.1, True) + W.at(u, 0.1, False))
        if s.get("lamps"):
            lx, ly = s["lamps"]
            p = Vector((lx, hand * ly, W.z(min(ly, W.tip))))
        m.use(f"wing.{nm}")
        m.ball(port(p), (0.07, 0.035, 0.03), paint, 8, 4)
    m.use("tail")
    m.ball((hull.back - 0.01, 0.0, hull.z(-hull.back)), 0.035, "lamp_white", 8, 4)


def bomb_bay(m, f, s, G, hull):
    """A bomber's bay behind its two doors: the doors' inner faces, the dark of the bay, and the bombs hung in it."""
    if not f.bay:
        return
    x0, x1 = f.bay
    half = math.radians(f.bay_half)
    lo, mid, hi = 1.5 * math.pi - half, 1.5 * math.pi, 1.5 * math.pi + half
    xs = [x for x in sorted(f.rings_x, reverse=True) if x1 - 1e-4 <= x <= x0 + 1e-4]
    steps = 3

    def spine(x):
        return Vector((x, 0.0, hull.z(-x)))
    # the inner face of each door, and the line where they meet
    for nm, a0, a1 in (("bay.port", lo, mid), ("bay.starboard", mid, hi)):
        m.use(nm)
        rows = [[port(hull.at(x, a0 + (a1 - a0) * k / steps, -0.02)) for k in range(steps + 1)] for x in xs]
        n = steps + 1
        m.add([q for row in rows for q in row], [(r * n + k, r * n + k + 1, (r + 1) * n + k + 1, (r + 1) * n + k) for r in range(len(xs) - 1) for k in range(steps)],
              "inside", True, lambda q: spine(q.x) - q)
    m.use("bay.port")
    m.strip([port(hull.at(x, mid)) for x in xs], [port(hull.out(x, mid)) for x in xs], 0.006, "line", lift=0.005)
    # her skin round the bay is scribed where the doors lie in it
    m.use("plane")
    for a in (lo, hi):
        m.strip([port(hull.at(x, a)) for x in xs], [port(hull.out(x, a)) for x in xs], 0.006, "line", lift=0.005)
    arc = [lo + (hi - lo) * k / (2 * steps) for k in range(2 * steps + 1)]
    for x in (x0, x1):
        m.strip([port(hull.at(x, a)) for a in arc], [port(hull.out(x, a)) for a in arc], 0.006, "line", lift=0.005)
    # the bay itself: two walls up from the doors' hinges, a roof, a bulkhead at each end
    xm = 0.5 * (x0 + x1)
    roof = min(hull.z(-x) - 0.05 * hull.h(-x) for x in xs)
    heart = Vector((xm, 0.0, hull.z(-xm) - 0.5 * hull.h(-xm)))
    inward = lambda q: heart - q
    edges = []
    for a in (lo, hi):
        foot = [port(hull.at(x, a, -0.03)) for x in xs]
        head = [Vector((q.x, q.y, roof)) for q in foot]
        n = len(xs)
        m.add(foot + head, [(k, k + 1, n + k + 1, n + k) for k in range(n - 1)], "inside", False, inward)
        edges.append(head)
    n = len(xs)
    m.add(edges[0] + edges[1], [(k, k + 1, n + k + 1, n + k) for k in range(n - 1)], "inside", False, inward)
    for x in (x0, x1):
        ring = [port(hull.at(x, a, -0.03)) for a in arc]
        ring += [Vector((ring[-1].x, ring[-1].y, roof)), Vector((ring[0].x, ring[0].y, roof))]
        c = sum(ring, Vector()) / len(ring)
        m.add(ring + [c], [(k, (k + 1) % len(ring), len(ring)) for k in range(len(ring))], "inside", False, inward)
    # her torpedo, where her bay can hold one instead of bombs
    if getattr(f, "bay_torpedo", None):
        long, r = f.bay_torpedo
        m.use("torpedo")
        at = Vector((xm + 0.5 * long, 0.0, hull.at(xm, lo).z + r + 0.06))
        m.lathe(at, -X, [(0.0, 0.0), (0.06, 0.1), (0.22, 0.2), (0.5, r), (long - 1.0, r), (long - 0.35, 0.1), (long, 0.03)], "torpedo", 14 if m.fine else 8)
        m.lathe(at - X * (long - 0.4), -X, [(0.0, r), (0.38, r)], "torpedo", 12 if m.fine else 8, caps=(False, False))
    # her bombs, in two rows on their racks
    m.use("bombs")
    wide = abs(hull.at(xm, lo).y)
    foot_z = hull.at(xm, lo).z
    rows = max(1, int((x0 - x1 - 0.5) / 1.5))
    sides = 10 if m.fine else 6
    for r in range(rows):
        x = xm + ((rows - 1) / 2.0 - r) * 1.5
        for sy in (1.0, -1.0):
            at = Vector((x + 0.62, 0.5 * wide * sy, foot_z + 0.45 * (roof - foot_z)))
            m.lathe(at, -X, [(0.0, 0.0), (0.07, 0.07), (0.2, 0.14), (0.4, 0.17), (0.85, 0.17), (1.1, 0.11), (1.24, 0.05)], "bomb", sides)
            m.lathe(at - X * 1.08, -X, [(0.0, 0.16), (0.2, 0.16)], "bomb", sides, caps=(False, False))
            m.use("plane")                                 # (her racks stay with her when the bombs go)
            m.rod(at - X * 0.55 + Z * 0.15, Vector((at.x - 0.55, at.y, roof)), 0.025, "gunmetal", sides=4)
            m.use("bombs")


# The machines whose flaps are full of holes (they were opened as brakes in the dive, and the holes kept her tail from
# shaking): (from how far out, to how far out, the shares of the chord the rows stand at).
HOLED = {}                                                  # (the Dauntless's is set by planes.py, from carrier_dauntless.py)


def holed_flaps(m, f, s, G, hull):
    if f.name not in HOLED or not m.fine:
        return
    y0, y1, rows = HOLED[f.name]
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        W = G["Wing"](s["wing"], hand)
        m.use(f"flap.{nm}" if f"flap.{nm}" in m.part_at else f"wing.{nm}")
        n = int((y1 - y0) / 0.15)
        for c in rows:
            for k in range(n + 1):
                y = y0 + (y1 - y0) * k / n
                for upper in (True, False):
                    p, out = W.at(y, c, upper), W.out(y, c, upper)
                    m.disc(port(p) + port(out) * 0.005, port(out), 0.032, "engine", 8)


def dive_brakes(m, f, s, G, hull):
    """A dive bomber's brakes, from `brakes` in her numbers (from, to out along her wing; where along its chord the
    slat's middle lies; half its breadth; how far it hangs below her skin): a slat under each wing on its brackets.
    It lay edge on to the air until the dive, then turned square to it."""
    if not s.get("brakes"):
        return
    y0, y1, c, half, drop = s["brakes"]
    n = 6 if m.fine else 2
    for hand in (-1.0, 1.0):
        W = G["Wing"](s["wing"], hand)
        m.use(("wing.port" if hand < 0.0 else "wing.starboard") if y0 > f.y_cut else "plane")
        rings = []
        for k in range(n + 1):
            y = y0 + (y1 - y0) * k / n
            p, out = port(W.at(y, c, False)), port(W.out(y, c, False))
            mid = p + out * drop
            rings.append([mid + X * half - out * 0.012, mid - X * half - out * 0.012, mid - X * half + out * 0.012, mid + X * half + out * 0.012])
            if m.fine and k % 3 == (1 if k < n else 0) or k in (0, n):
                for dx in (0.6 * half, -0.6 * half):
                    m.rod(p + X * dx - out * 0.01, mid + X * dx, 0.013, "under", sides=5)
        m.skin(rings, "under", (True, True), False)
    m.use("plane")


def rivets(m, f, s, G, hull):
    """Her skin's panels: rows of rivets, as small dark dots a shade under her paint, and the joints between panels, as
    fine lines a shade under that. Round the fuselage at every frame and along her stringers; along the spars and the
    ribs of her wings, her tailplane and her fin; round her engines' nacelles; over a raised cabin's roof. A joint
    runs beside every other frame and rib, and along the main stringers and spars. (Brian, 2026-11-19, of two models
    of a B-17 he sent pictures of: "use this 3d model is our bench mark for all airplanes", "the level of detail im
    looking for": a grid of seams and rivet lines over all her skin. The plain model has none.)"""
    if not m.fine:
        return
    big = bool(s.get("engines"))
    r, gap = (0.006, 0.11) if big else (0.0045, 0.08)       # (flush dots of paint 9 to 12 mm across; they were half as big again and read as studs)
    seam = 0.0034 if big else 0.0028                       # (half as wide as a joint is drawn)
    dots = {}

    def joint(part, run):
        """A joint along a row of (point, facing), left out where an entry is None and broken where her paint changes."""
        piece, paint = [], None

        def flush():
            if len(piece) >= 2:
                m.use(part)
                m.strip([port(q[0]) for q in piece], [port(q[1]) for q in piece], seam, paint, lift=0.001)
            del piece[:]
        for got in run:
            if got is None:
                flush()
                continue
            own = "seam_under" if got[2] else "seam"
            if piece and own != paint:
                piece.append(got)
                flush()
            paint = own
            piece.append(got)
        flush()

    bare_under = s.get("rivets_under") is False            # (the art director, 2026-11-19: none at all on the Lancaster's black undersides)
    # (`geodetic`: (pitch, half width, shade). Doped fabric over a geodetic frame, the Wellington's: no rivet or joint on
    #  her fuselage or her flying surfaces, but the frame's two sets of members crossing under the fabric, each at 45
    #  degrees to her length: "a quiet lattice in the skin, not a cage". A member is her coat itself, in whichever of
    #  its colours lies there, a shade darker (as soot darkens it: `shade` of the way to it), and a hair paler than
    #  her black on her black. The members round her fuselage are as many at her tail as at her widest, closer
    #  together as she slims, as they were.)
    geo = s.get("geodetic")
    coat_blend = G["coat_of"](s).blend if geo else None

    def member(part, piece):
        """A member of her geodetic frame along a row of (point, facing, under), laid on her skin a run of one paint at a time."""
        k = 0
        while k < len(piece) - 1:
            j = k
            while j + 1 < len(piece) and piece[j + 1][2] == piece[k][2]:
                j += 1
            run = piece[k:j + 2]
            pts, nrm = [port(q[0]) for q in run], [port(q[1]).normalized() for q in run]
            verts, faces = [], []
            for i, (p, n) in enumerate(zip(pts, nrm)):
                side = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).cross(n)
                side = side.normalized() if side.length > 1e-9 else Y.copy()
                verts += [p + n * 0.001 + side * geo[1], p + n * 0.001 - side * geo[1]]
            for i in range(len(pts) - 1):
                q = (2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2)
                if (verts[q[1]] - verts[q[0]]).cross(verts[q[2]] - verts[q[0]]).dot(nrm[i]) < 0.0:
                    q = tuple(reversed(q))
                faces.append(q)
            if run[0][2]:
                m.raw(verts, faces, ["rivet_under"] * len(faces), part)
            else:
                m.raw(verts, faces, ["upper"] * len(faces), part, blend=[coat_blend(q[0]) for q in run for _ in (0, 1)], soot=[geo[2]] * len(verts))
            k = j + 1

    def lattice_run(run, dark=False):
        """Her geodetic frame's members along a row of (part, (point, facing)) or None, broken where its part changes.
        A member is in her under colour where her skin faces down past `under_from` (or where `dark`), and in her
        coat's where it faces up past it; within a few degrees of that line, where her coat changes from face to face
        of her skin, there is none."""
        under_line = s.get("under_from", -0.3)
        run = [None if got is None or (not dark and abs(got[1][1].z - under_line) < 0.08) else (got[0], (got[1][0], got[1][1], dark or got[1][1].z < under_line))
               for got in run]
        piece, part = [], None
        for got in run + [None]:
            if got is None or (piece and got[0] != part):
                if piece:
                    member(part, piece)
                piece = [piece[-1], got[1]] if got is not None and piece else ([got[1]] if got is not None else [])
                part = got[0] if got is not None else None
                continue
            part = got[0]
            piece.append(got[1])

    def dot(part, paint, p, n, t):
        if bare_under and paint == "rivet_under":
            return
        dots.setdefault((part, paint), []).append((port(p), port(n), port(t)))
    # -- the fuselage
    under_from = s.get("under_from", -0.3)
    hoods = [(max(q[0] for q in st) + 0.08, min(q[0] for q in st) - 0.08) for st, hoops, spine in s.get("hoods", [])]
    glazed = s.get("glazed")
    if big:
        x_front = hull.front - 0.7
    else:
        x_front = (s.get("nose_back") if s.get("nose_back") is not None else hull.front - f.long * 0.2) - 0.2
    if glazed:
        x_front = min(x_front, glazed[1] - 0.15)
    x_back = hull.back + 0.4
    bands = s.get("glass_bands", [])

    cab_ = s.get("cab")
    ramps = [(cab_[0], cab_[1]), (cab_[2], cab_[3])] if cab_ else []     # (fitout.cab_hull: (higher x, lower x) of each)

    def skin_at(x, a, lattice=False):
        """A point of her fuselage's skin, the way it faces and whether it is her under surface; None where neither a
        rivet nor a joint belongs (nor, with `lattice`, a member of her geodetic frame)."""
        if geo and not lattice:
            return None                                    # (her fabric: no rivet, no joint)
        deg = math.degrees(a) % 360.0
        if math.sin(a) > 0.0 and any(lo + 1e-4 < x < hi - 1e-4 for hi, lo in ramps):
            return None                                    # (where her tail gunner's box rises out of her, fore and aft: her mesh is twisted there)
        if 38.0 < deg < 142.0 and any(lo <= x <= hi for hi, lo in hoods):
            return None                                    # (under her hood)
        if f.bay and f.bay[1] - 0.06 <= x <= f.bay[0] + 0.06 and abs(deg - 270.0) < f.bay_half + 2.5:
            return None                                    # (her bomb doors)
        cab = getattr(f, "cockpit", None)
        if cab and cab[2] < 38.0 and cab[1] - 0.03 < x < cab[0] + 0.03 and math.degrees(math.asin(math.sin(a))) > cab[2] - 4.0:
            return None                                    # (a bomber's cabin, open under its greenhouse from her sills up)
        p, n = hull.at(x, a, 0.003), hull.out(x, a)
        if f.waist_hole(p, 0.03) or f.pane_at(p, 0.03):
            return None                                    # (an open window, a pane of glass)
        for (x0, x1, half_high, *high) in bands:
            if x1 - 0.05 < x < x0 + 0.05 and abs(n.y) > 0.6 and abs(p.z - hull.z(-x) - hull.h(-x) * (high[0] if high else 0.3)) < half_high + 0.04:
                return None                                # (her cabin windows)
        return p, n, n.z < under_from

    def body_dot(x, a, along):
        got = skin_at(x, a)
        if got is None:
            return
        p, n, under = got
        t = X if along else (hull.at(x, a + 0.02) - hull.at(x, a - 0.02))
        dot("tail" if x < f.x_cut else "plane", "rivet_under" if under else "rivet", p, n, t)
    step = min(max(f.long / 42.0, 0.42), 0.56)             # (a frame every half metre or so)
    n_rings = max(2, int((x_front - x_back) / step))
    ring_xs = [x_front - (x_front - x_back) * k / n_rings for k in range(n_rings + 1)]
    for x in ring_xs:
        round_ = 2.0 * math.pi * 0.5 * (hull.w(-x) + hull.h(-x))
        n = max(8, int(round_ / gap))
        for i in range(n):
            body_dot(x, 2.0 * math.pi * (i + 0.5) / n, False)
    n_along = max(4, int((x_front - x_back) / gap))
    for deg in (90.0, 270.0, 64.0, 116.0, 38.0, 142.0, 12.0, 168.0, -14.0, 194.0, -40.0, 220.0, -66.0, 246.0):
        for i in range(n_along):
            body_dot(x_front - (x_front - x_back) * (i + 0.5) / n_along, math.radians(deg), True)
    # (a joint round her beside every other frame, and along four of her stringers)
    for x in ring_xs[1::2]:
        x = x + 0.022
        joint("tail" if x < f.x_cut else "plane", [skin_at(x, 2.0 * math.pi * k / 56.0) for k in range(57)])
    n_seam = max(4, int((x_front - x_back) / 0.24))
    # (where her skin turns a corner along her length, at either end of her tail gunner's box (fitout.cab_hull), a
    #  joint has a point on each ring of the corner, or it runs straight across the corner, off her skin)
    creases = [q for q in (s.get("cab") or ())[:4] if x_back < q < x_front]
    for deg in (38.0, 142.0, -40.0, 220.0):
        a = math.radians(deg + 1.2)
        xs = sorted([x_front - (x_front - x_back) * k / n_seam for k in range(n_seam + 1)] + creases, reverse=True)
        joint("plane", [skin_at(x, a) for x in xs if x >= f.x_cut - 0.13])
        joint("tail", [skin_at(x, a) for x in xs if x <= f.x_cut + 0.13])

    # -- her geodetic frame under her fabric, round her fuselage: one set of members winding each way
    if geo:
        x_hi, x_lo = hull.front - 0.35, hull.back + 0.12

        def girth(x):
            a_, b_ = hull.w(-x), hull.h(-x)
            return math.pi * (3.0 * (a_ + b_) - math.sqrt((3.0 * a_ + b_) * (a_ + 3.0 * b_)))
        n_geo = 2 * max(4, int(round(0.5 * max(girth(x_hi - (x_hi - x_lo) * k / 40.0) for k in range(41)) / geo[0])))
        n_x = max(8, int((x_hi - x_lo) / 0.08))
        xs = [x_hi - (x_hi - x_lo) * k / n_x for k in range(n_x + 1)]
        turn = [0.0]                                       # (how far round her a member has wound, from her front)
        for x0_, x1_ in zip(xs, xs[1:]):
            turn.append(turn[-1] + (x0_ - x1_) * math.pi / girth(0.5 * (x0_ + x1_)) * 2.0)
        for i in range(n_geo):
            for way in (1.0, -1.0):
                a0 = 0.5 * math.pi + 2.0 * math.pi * (i + 0.5) / n_geo
                lattice_run([(("tail" if x < f.x_cut else "plane"), got[:2]) if got else None
                             for x, w in zip(xs, turn) for got in [skin_at(x, a0 + way * w, True)]])

    # -- a raised cabin and the deck behind it: frames over its roof, and rows along it
    cab = s.get("cabin")
    if cab and s.get("hoods"):
        hood = hood_body(G, s)
        hatch, brow = cab.get("hatch"), cab.get("brow")

        def roof_at(x, deg):
            if deg > 90.0:
                return roof_at_side(x, 180.0 - deg, True)
            return roof_at_side(x, deg, False)

        def roof_at_side(x, deg, far):
            low = x >= cab["back"] and deg < hood.deg_of(x, cab["sill"]) - 4.0      # (her side under her windows)
            if x >= cab["back"] and not low and deg < hood.deg_of(x, cab["top"]) + 7.0:
                return None                                # (her side windows)
            if (x > hood.roof_from - 0.06 or cab.get("roof")) and not low:
                return None                                # (her windscreen; a greenhouse's glass)
            if hatch and hatch[1] - 0.05 < x < hatch[0] + 0.05 and deg > hatch[2] - 4.0:
                return None
            if brow and brow[1] - 0.05 < x < brow[0] + 0.05 and brow[2] - 4.0 < deg < brow[3] + 4.0:
                return None
            a = math.radians(180.0 - deg if far else deg)
            p, n = hood.at(x, a, 0.003), hood.out(x, a)
            if p.z < body_top(hull, x, p.y) + 0.02 or (low and p.z > hood.pane_z(x, p.y) - 0.05):
                return None                                # (inside her fuselage; at her windscreen's foot)
            if any((port(p) - c).length < 1.08 * max(radii) for k, c, radii in f.turrets):
                return None                                # (under a turret)
            return p, n, False
        x_hi, x_lo = hood.front - 0.05, hood.back + 0.2
        for x in ring_xs:
            if not x_lo < x < x_hi:
                continue
            round_ = math.pi * 0.5 * (hood.w(-x) + hood.h(-x))
            n = max(6, int(round_ / gap))
            for i in range(n):
                got = roof_at(x, 180.0 * (i + 0.5) / n)
                if got:
                    dot("plane", "rivet", got[0], got[1], hood.at(x, math.radians(180.0 * (i + 0.5) / n) + 0.02) - hood.at(x, math.radians(180.0 * (i + 0.5) / n) - 0.02))
        n_along = max(4, int((x_hi - x_lo) / gap))
        for deg in (3.0, 9.0, 16.0, 52.0, 76.0, 104.0, 128.0, 164.0, 171.0, 177.0):
            for i in range(n_along):
                got = roof_at(x_hi - (x_hi - x_lo) * (i + 0.5) / n_along, deg)
                if got:
                    dot("plane", "rivet", got[0], got[1], X)
        for x in ring_xs[1::2]:
            if x_lo < x + 0.022 < x_hi:
                joint("plane", [roof_at(x + 0.022, 180.0 * k / 28.0) for k in range(29)])
        n_seam = max(4, int((x_hi - x_lo) / 0.24))
        for deg in (53.2, 126.8):
            joint("plane", [roof_at(x_hi - (x_hi - x_lo) * k / n_seam, deg) for k in range(n_seam + 1)])

    # -- her engines' nacelles: a ring of rivets at each frame, and a joint behind the cowling and one further aft
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            part = ("wing.starboard" if hand > 0.0 else "wing.port") if out > f.y_cut else "plane"
            nacelle = nacelle_of(G, lead, z, is_radial, back, out * hand, s)
            W = G["Wing"](s["wing"], hand)

            def cowl_at(x, a):
                p, n = nacelle.at(x, a, 0.003), nacelle.out(x, a)
                u = abs(p.y)
                if W.root <= u <= W.tip:
                    c = (p.x - W.lead(u)) / (W.trail(u) - W.lead(u))
                    if 0.0 < c < 1.0 and W.at(u, c, False).z - 0.015 < p.z < W.at(u, c, True).z + 0.015:
                        return None                        # (inside her wing)
                return p, n, n.z < under_from
            first = lead + (1.28 if is_radial else 1.6)
            xs = [first] + [lead + 0.3 - 0.5 * k for k in range(12) if lead + 0.3 - 0.5 * k > lead - 0.72 * back]
            for x in xs:
                n = max(8, int(2.0 * math.pi * 0.5 * (nacelle.w(-x) + nacelle.h(-x)) / gap))
                for i in range(n):
                    a = 2.0 * math.pi * (i + 0.5) / n
                    got = cowl_at(x, a)
                    if got:
                        dot(part, "rivet_under" if got[2] else "rivet", got[0], got[1], nacelle.at(x, a + 0.02) - nacelle.at(x, a - 0.02))
            for x in xs[:2]:
                joint(part, [cowl_at(x + 0.022, 2.0 * math.pi * k / 40.0) for k in range(41)])

    # -- a jet's pods: a ring of rivets at each frame
    for pod, out in f.pods:
        part = ("wing.starboard" if out > 0.0 else "wing.port") if abs(out) > f.y_cut else "plane"
        n = max(8, int(2.0 * math.pi * 0.4 / gap))
        for k in range(1, 9):
            x = pod.front - 0.2 - (pod.front - pod.back - 0.5) * k / 9.0
            for i in range(n):
                a = 2.0 * math.pi * (i + 0.5) / n
                p, nn = pod.at(x, a, 0.003), pod.out(x, a)
                dot(part, "rivet_under" if nn.z < under_from else "rivet", p, nn, pod.at(x, a + 0.02) - pod.at(x, a - 0.02))

    # -- her flying surfaces
    def ridged(kind, u, c):
        """Whether a place on the upper face of a flying surface is under her corrugated skin's ridges (`corrugated`:
        see corrugation): no rivet or joint is laid there, where the ridges would bury it."""
        for k, a, b, (ca0, cb0), (ca1, cb1), pitch in s.get("corrugated", ()):
            if k == kind and a - 0.02 <= u <= b + 0.02:
                t = min(max((u - a) / max(b - a, 1e-9), 0.0), 1.0)
                if ca0 + (ca1 - ca0) * t - 0.02 <= c <= cb0 + (cb1 - cb0) * t + 0.01:
                    return True
        return False

    def rows(sections, kind, c_max, spars, rib_gap, upright=False, side=0.0, joints=(), own=None):
        """Her rows on a flying surface. `own`: (from, to along its span, and the rows' c_max, spars and joints there),
        where a surface of her own numbers is cut from it (see own_of); outside that span the rows are as given."""
        for hand in ((1.0,) if upright else (1.0, -1.0)):
            W = G["Wing"](sections, hand, upright, side)
            xm = 0.5 * (W.lead(W.root) + W.trail(W.root))
            if upright:
                u0 = max(W.root, body_top(hull, xm) + 0.06)
            else:
                u0 = max(W.root, hull.w(-min(max(xm, hull.back), hull.front)) + 0.08)
            u1 = W.tip - max(0.12, 0.035 * (W.tip - W.root))
            if u1 - u0 < 0.3:
                continue
            if own is not None:
                lo_u = W.root + (W.tip - W.root) * own[0]
                hi_u = W.tip + 1.0e-3 if own[1] >= 1.0 else W.root + (W.tip - W.root) * own[1]     # (as convert() cuts it)

            def on(u):
                """Whether a place along the span is on her own surface's span."""
                return own is not None and lo_u < u < hi_u

            def part_of(u):
                if kind != "wing":
                    return "tail"
                return ("wing.starboard" if hand > 0.0 else "wing.port") if u > f.y_cut else "plane"

            def put(u, c, t_of):
                for upper in (True, False):
                    if upper and ridged(kind, u, c):
                        continue                           # (under the ridges of her corrugated skin: see ridged)
                    p, n = W.at(u, c, upper, 0.003), W.out(u, c, upper)
                    dot(part_of(u), "rivet" if (upper or upright) and not (upright and bare_under and s.get("fins_under")) else "rivet_under", p, n, t_of(u, c, upper))
            chordwise = lambda u, c, up: W.at(u, min(c + 0.02, 1.0), up) - W.at(u, max(c - 0.02, 0.0), up)
            if geo:                                        # (her fabric: the members of her geodetic frame, ahead of any hinge)
                lattice_on(W, u0, u1, lambda u: own[2] if on(u) else c_max, part_of, upright and s.get("fins_under"))
                continue
            spanwise = lambda u, c, up: W.at(min(u + 0.05, W.tip), c, up) - W.at(max(u - 0.05, W.root), c, up)
            n_ribs = max(1, int((u1 - u0) / rib_gap))
            for k in range(n_ribs):
                u = u0 + (u1 - u0) * (k + 0.5) / n_ribs
                c_rib = own[2] if on(u) else c_max
                n = max(2, int((W.trail(u) - W.lead(u)) * (c_rib - 0.05) / gap * (-1.0 if W.trail(u) < W.lead(u) else 1.0)))
                for i in range(n):
                    put(u, 0.05 + (c_rib - 0.05) * (i + 0.5) / n, chordwise)
                if k % 2 == 0:                             # (a joint beside every other rib)
                    us = min(u + 0.022, u1)
                    c_rib = own[2] if on(us) else c_max
                    cs = [0.03 + (c_rib - 0.03) * i / 10.0 for i in range(11)]
                    for upper in (True, False):
                        joint(part_of(us), [None if upper and ridged(kind, us, c) else (W.at(us, c, upper, 0.003), W.out(us, c, upper), not (upper or upright)) for c in cs])
            n = max(2, int((u1 - u0) / gap))
            for c in spars:
                for i in range(n):
                    u = u0 + (u1 - u0) * (i + 0.5) / n
                    if on(u) and c not in own[3]:
                        continue                           # (aft of her own surface's hinge)
                    put(u, c, spanwise)
            cuts = [u0] + ([f.y_cut] if kind == "wing" and u0 < f.y_cut < u1 else []) + [u1]
            for c in joints:                               # (and along her spars)
                for a, b in zip(cuts, cuts[1:]):
                    runs = [(a, b)]
                    if own is not None and c not in own[4]:
                        # (aft of her own surface's hinge: only beyond each end of its span)
                        runs = [(p, q) for p, q in ((a, min(b, lo_u)), (max(a, hi_u), b)) if q - p > 0.05]
                    for p, q in runs:
                        n = max(2, int((q - p) / 0.3))
                        us = [p + (q - p) * i / n for i in range(n + 1)]
                        for upper in (True, False):
                            joint(part_of(0.5 * (p + q)), [None if upper and ridged(kind, u, c + 0.006) else (W.at(u, c + 0.006, upper, 0.003), W.out(u, c + 0.006, upper), not (upper or upright)) for u in us])

    def own_of(key, c_max, spars, joints):
        """Her rows on a tailplane or fin where her numbers give her own elevator or rudder (`elevator`, `rudder`): over
        its span they stop ahead of its hinge, so that none is left standing on the fixed part when it swings."""
        if key not in s:
            return None
        cut = f.hinges[key]
        return (s[key][0], s[key][1], min(c_max, cut - 0.06), tuple(c for c in spars if c < cut - 0.04), tuple(c for c in joints if c < cut - 0.04))

    def lattice_on(W, u0, u1, c_top, part_of, dark):
        """Her geodetic frame under the fabric of a flying surface: two sets of members, each at 45 degrees to her
        length, on both its faces (`dark`: a fin in her under colour), from just behind its leading edge to `c_top`
        along its chord."""
        n_u = max(4, int((u1 - u0) / 0.08))
        us = [u0 + (u1 - u0) * i / n_u for i in range(n_u + 1)]
        gap_x = geo[0] * math.sqrt(2.0)
        for way in (1.0, -1.0):
            ends = [x - way * u for u in us for x in (W.lead(u), W.trail(u))]
            for k in range(int(math.floor(min(ends) / gap_x)), int(math.ceil(max(ends) / gap_x)) + 1):
                for upper in (True, False):
                    run = []
                    for u in us:
                        c = ((k + 0.5) * gap_x + way * u - W.lead(u)) / (W.trail(u) - W.lead(u))
                        run.append((part_of(u), (W.at(u, c, upper, 0.003), W.out(u, c, upper))) if 0.04 <= c <= c_top(u) else None)
                    lattice_run(run, bool(dark))

    rib = min(max(f.half / 26.0, 0.45), 0.62)              # (a rib every half metre or so)
    rows(s["wing"], "wing", 0.66, (0.1, 0.24, 0.4, 0.56), rib, joints=(0.24, 0.56))
    rows(s["tail"], "tail", 0.56, (0.12, 0.3, 0.48), rib, joints=(0.3,), own=own_of("elevator", 0.56, (0.12, 0.3, 0.48), (0.3,)))
    own = own_of("rudder", 0.54, (0.12, 0.3, 0.48), (0.3,))
    for fin_sections, fin_side in f.fins:
        for side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            rows(fin_sections, "fin", 0.54, (0.12, 0.3, 0.48), rib, True, side, joints=(0.3,), own=own)

    # -- laid a part and a paint at a time: each one a small six-sided dot lying in the skin (four-sided underneath,
    #    where nobody looks at her from a hand's breadth away)
    for (part, paint), each in dots.items():
        sides = 4 if paint.endswith("_under") else 6
        ring = [(math.cos(2.0 * math.pi * i / sides) * r, math.sin(2.0 * math.pi * i / sides) * r) for i in range(sides)]
        verts, faces = [], []
        for p, n, t in each:
            t = t - n * t.dot(n)
            if t.length < 1e-9:
                continue
            t.normalize()
            w = n.cross(t)
            k = len(verts)
            verts += [p + t * c + w * d for c, d in ring]
            faces.append(tuple(range(k, k + sides)))
        m.use(part)
        m.add(verts, faces, paint, False)
    m.use("plane")
    f.rivets = sum(len(v) for v in dots.values())


def corrugation(m, f, s, G, hull):
    """Her corrugated skin (`corrugated` in her numbers: the Devastator's), on the fine model only: fine ridges in the
    skin of her upper surfaces, running with the air, one every `pitch` along the span where her drawing hatches it.
    Each is a low ridge in her skin's own paint, its two faces leaning a little apart, so that light and shade give it
    as the corrugation it is and never as a stripe of paint. Each region is (`wing` or `tail`, from, to along its span,
    (from, to) along its chord at the first, (from, to) at the second, as shares of the chord, the pitch): her numbers
    keep each region ahead of the hinge of any flap, aileron or elevator over it, so that no ridge stands on a moving part.
    A ridge bends where her skin does, at the rings of her surface's chord (and nowhere else): so it lies on her skin
    all along, a hair under it at its feet, and never sinks into it where her skin is faceted. None stands under a
    star on her upper wing (`wing_marks`): the star is laid on her skin there, flat and whole, and a ridge under it
    would crumple it."""
    if not m.fine or not s.get("corrugated"):
        return
    sink, crest = 0.0015, 0.0045
    rings = G["BATTLE_CHORDS"][True]
    for kind, u0, u1, (a0, b0), (a1, b1), pitch in s["corrugated"]:
        sections = s["wing"] if kind == "wing" else s["tail"]
        half = 0.47 * pitch
        n = max(1, int(round((u1 - u0) / pitch)))
        for hand in (1.0, -1.0):
            W = G["Wing"](sections, hand)
            stars = [(out, x, radius) for (out, x, radius, upper, lower, both_upper, both_lower) in s.get("wing_marks", [])
                     if kind == "wing" and upper and (both_upper or hand < 0.0)]
            for k in range(n):
                u = u0 + (u1 - u0) * (k + 0.5) / n
                t = (u - u0) / max(u1 - u0, 1e-9)
                spans = [(a0 + (a1 - a0) * t, b0 + (b1 - b0) * t)]
                for out, x, radius in stars:
                    if abs(u - out) < radius:
                        dx = math.sqrt(radius * radius - (u - out) ** 2)
                        lo, hi = sorted((x + sgn * dx - W.lead(u)) / (W.trail(u) - W.lead(u)) for sgn in (1.0, -1.0))
                        spans = [(p_, q_) for a_, b_ in spans for p_, q_ in ((a_, min(b_, lo)), (max(a_, hi), b_))]
                for ca, cb in spans:
                    if cb - ca < 0.03:
                        continue
                    ridge(m, f, W, kind, hand, u, ca, cb, rings, half, sink, crest)
    m.use("plane")


def ridge(m, f, W, kind, hand, u, ca, cb, rings, half, sink, crest):
    """One ridge of her corrugated skin (see corrugation): at `u` along the span, from `ca` to `cb` along the chord."""
    cs = [ca]
    for r in rings:
        if cs[-1] + 0.025 < r < cb - 0.015:
            cs.append(r)                                    # (a ring this close behind the last is a hinge's pair: her skin hardly bends there)
    cs.append(cb)
    steps = len(cs) - 1
    verts, facing = [], []
    for c in cs:
        p, nrm = W.at(u, c, True), W.out(u, c, True)
        sp = W.at(min(u + 0.01, W.tip), c, True) - W.at(max(u - 0.01, W.root), c, True)
        sp = sp - nrm * sp.dot(nrm)
        if sp.length < 1e-9:
            sp = Vector((0.0, 1.0, 0.0))
        sp.normalize()
        verts += [port(p - sp * half - nrm * sink), port(p + nrm * crest), port(p + sp * half - nrm * sink)]
        facing.append(port(nrm))
    faces = []
    for j in range(steps):
        a, b = 3 * j, 3 * (j + 1)
        faces += [(a, a + 1, b + 1, b), (a + 1, a + 2, b + 2, b + 1)]
    faces += [(0, 1, 2), (3 * steps, 3 * steps + 1, 3 * steps + 2)]
    m.use(("wing.starboard" if hand > 0.0 else "wing.port") if kind == "wing" and u > f.y_cut else "plane" if kind == "wing" else "tail")
    pts = [verts[3 * j + 1] for j in range(len(cs))]
    m.add(verts, faces, "upper", True, lambda q, pts=pts, facing=facing: facing[min(range(len(pts)), key=lambda i: (pts[i] - q).length_squared)])


BOOTS = {"B17"}


def boots(m, f, s, G, hull):
    """The black rubber along the leading edges of an American heavy bomber's wings and tail."""
    if f.name not in BOOTS:
        return

    def lay(sections, kind, deep, upright=False, side=0.0):
        for hand in ((1.0,) if upright else (1.0, -1.0)):
            W = G["Wing"](sections, hand, upright, side)
            xm = 0.5 * (W.lead(W.root) + W.trail(W.root))
            u0 = max(W.root, body_top(hull, xm) + 0.05) if upright else max(W.root, hull.w(-min(max(xm, hull.back), hull.front)) + 0.05)
            u1 = W.tip - max(0.2, 0.05 * (W.tip - W.root))
            cuts = [u0] + ([f.y_cut] if kind == "wing" and u0 < f.y_cut < u1 else []) + [u1]
            for a, b in zip(cuts, cuts[1:]):
                n = max(2, int((b - a) / (0.3 if m.fine else 0.9)))
                us = [a + (b - a) * k / n for k in range(n + 1)]
                cs = [(deep, False), (deep * 0.35, False), (0.0, True), (deep * 0.35, True), (deep, True)]
                verts = [port(W.at(u, c, up, 0.005)) for u in us for c, up in cs]
                faces = [(i * 5 + j, i * 5 + j + 1, (i + 1) * 5 + j + 1, (i + 1) * 5 + j) for i in range(n) for j in range(4)]
                heart = lambda q: q - port(0.5 * (W.at(min(max(q.z if upright else abs(q.y), W.root), W.tip), 0.25, True) + W.at(min(max(q.z if upright else abs(q.y), W.root), W.tip), 0.25, False)))
                m.use(("tail" if kind != "wing" else (("wing.starboard" if hand > 0.0 else "wing.port") if 0.5 * (a + b) > f.y_cut else "plane")))
                m.add(verts, faces, "boot", True, heart)
    lay(s["wing"], "wing", 0.05)
    lay(s["tail"], "tail", 0.07)
    for fin_sections, fin_side in f.fins:
        for side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            lay(fin_sections, "fin", 0.06, True, side)
    m.use("plane")


def liner(m, f, hull, x_hi, x_lo, open_at, inset=0.035, paint="inside"):
    """Her skin again from the inside, a little in from it and dark, left open where open_at(how far along her, degrees
    up from level) says: what is seen through a window is then her compartment, and not the far side's paint. Gives
    the rings it runs over."""
    n = f.around
    step = 2.0 * math.pi / n
    xs = [x for x in sorted(f.rings_x, reverse=True) if x_lo - 1e-4 <= x <= x_hi + 1e-4]
    verts = [port(hull.at(x, k * step, -inset)) for x in xs for k in range(n)]
    faces = []
    for i in range(len(xs) - 1):
        xm = 0.5 * (xs[i] + xs[i + 1])
        for k in range(n):
            high = math.degrees(math.asin(math.sin((k + 0.5) * step)))
            if open_at(xm, high):
                continue
            faces.append((i * n + k, i * n + (k + 1) % n, (i + 1) * n + (k + 1) % n, (i + 1) * n + k))
    m.add(verts, faces, paint, True, lambda q: Vector((q.x, 0.0, hull.z(-q.x))) - q)
    return xs


def waist(m, f, s, G, hull):
    """A heavy bomber's open waist windows: the dark of the compartment seen through them, a rim round each, and a
    gunner standing at each gun with its breech and its ammunition box."""
    if not f.waist:
        return
    x0, x1, a0, a1 = f.waist
    m.use("tail" if 0.5 * (x0 + x1) < f.x_cut else "plane")
    # the compartment, with the same two windows (in the paler green of a room the sun comes into, where her numbers
    # frame her windows: so that her gunner is seen at his gun, and not lost in the dark)
    room = "room" if s.get("framed_windows") else "inside"
    xs = liner(m, f, hull, x0 + 0.62, x1 - 0.62, lambda x, high: x1 < x < x0 and a0 < high < a1, paint=room)
    for x, way in ((xs[0], -X), (xs[-1], X)):
        m.disc((x, 0.0, hull.z(-x)), way, hull.h(-x) * 0.97, room, 24, oval=hull.w(-x) / hull.h(-x))
    # her floor, low in her, where her gunners stand: as low as leaves it wide enough for a man to stand at each side,
    # and no lower than a standing man's feet (it was at half her height, and they ended at the chest)
    xm = 0.5 * (x0 + x1)
    heads = [base.z + 0.2 for nm, base, tip in f.flex if x1 - 0.3 < base.x < x0 + 0.3 and abs(base.y) > 0.5 * hull.w(-base.x)]
    zf = hull.z(-xm) - 0.86 * hull.h(-xm)
    if heads:
        zf = max(zf, min(heads) - (HIP_STAND + THIGH + SHIN + 0.09))
    if m.fine:
        deck(m, hull, xs[0], xs[-1], zf, room)
    else:                                                # (far: the gunners to the chest, over a floor at half her height)
        m.box((xm, 0.0, hull.z(-xm) - 0.55 * hull.h(-xm)), (xs[0] - xs[-1], 1.5 * hull.w(-xm), 0.04), room)
    # a rim round each window
    wx = [x for x in sorted(f.rings_x, reverse=True) if x1 - 1e-4 <= x <= x0 + 1e-4]
    for hand in (1.0, -1.0):
        ang = (lambda a: math.radians(a)) if hand > 0 else (lambda a: math.radians(180.0 - a))
        for a in (a0, a1):
            m.strip([port(hull.at(x, ang(a))) for x in wx], [port(hull.out(x, ang(a))) for x in wx], 0.016, "gunmetal", lift=0.006)
        arc = [a0 + (a1 - a0) * k / 6.0 for k in range(7)]
        for x in (x0, x1):
            m.strip([port(hull.at(x, ang(a))) for a in arc], [port(hull.out(x, ang(a))) for a in arc], 0.016, "gunmetal", lift=0.006)
    # the gunners and their guns
    for nm, base, tip in f.flex:
        if not (x1 - 0.3 < base.x < x0 + 0.3 and abs(base.y) > 0.5 * hull.w(-base.x)):
            continue
        d = tip - base
        flat = Vector((d.x, d.y, 0.0)).normalized()
        m.use("tail" if base.x < f.x_cut else "plane")
        head = base - flat * 0.62 + Z * 0.2
        at = figure(m, head, math.degrees(math.atan2(flat.y, flat.x)), vest=False)
        body = m.parts[m.cur].name
        m.rod(base - flat * 0.34 - Z * 0.02, base - flat * 0.34 - Z * 0.6, 0.03, "gunmetal", sides=6)           # (the post his gun swings on)
        m.box(base - flat * 0.62 + flat.cross(Z) * 0.3 - Z * 0.42, (0.3, 0.16, 0.24), "gunmetal")               # (his ammunition box)
        if m.fine:
            trunk(m, at, sit=False)                      # (standing at his gun, the whole of him, down to her floor)
            legs(m, at, "stand", zf, inside=lambda p: _in(hull, p, 0.08))
        if m.fine:
            m.use(nm)
            dn = d.normalized()
            m.rod(base - dn * 0.42, base + dn * 0.05, 0.045, "gunmetal", sides=8)                              # (the breech and its two grips)
            across = dn.cross(Z).normalized()
            grips = [base - dn * 0.45 + across * 0.07 * sy for sy in (-1.0, 1.0)]
            for g in grips:
                m.rod(g - Z * 0.06, g + Z * 0.06, 0.016, "gunmetal", sides=5)
            left = at(0.0, 1.0, 0.0) - at(0.0, 0.0, 0.0)
            grips.sort(key=lambda g: -(g - base).dot(left))
            m.use(body)
            arms(m, at, grips, fore=nm)                    # (both his hands on his gun)
    m.use("plane")


# A gunner who sits in her tail behind his windows: (how far along her, how high above her middle).
REAR_CREW = {"B17": (-12.72, 0.46), "B29": (-18.56, 0.12), "Betty": (-10.85, 0.05)}


def tail_gunner(m, f, s, G, hull):
    if f.name not in REAR_CREW:
        return
    x, up = REAR_CREW[f.name]
    m.use("tail")
    at = figure(m, (x, 0.0, hull.z(-x) + up), 180.0, vest=False)
    xb = x + 0.68
    if s.get("cab"):
        # (in her own section: an oval would leave the box's upper corners open, or stand out through its sides and roof;
        #  the table's model's wall on its own 14 rings, so that its corners stand on the box's)
        wall(m, hull, xb, -X, "inside", inset=0.02, sides=40 if m.fine else 14)
    else:
        m.disc((xb, 0.0, hull.z(-xb)), -X, hull.h(-xb) * 0.96, "inside", 20 if m.fine else 10, oval=hull.w(-xb) / hull.h(-xb))     # (nobody looks along her fuselage)
    # (he kneels: his knees on pads on her floor, aft of him, his shins back along it, and his seat under him; her floor
    #  as low as her skin lets it lie under all of that, so that he kneels upright where her tail is deep enough)
    zf = max(bottom_at(hull, q, d) for q in (x - 0.4, x, x + 0.45) for d in (-0.17, 0.17)) + 0.05
    zf = max(zf, hull.z(-x) + up - HIP_SIT - THIGH - 0.07)
    if not (m.fine and hull.z(-x) + up - zf > 0.95):
        # (far, and where her tail is too low for a man to kneel in, as the B-29's and the Betty's are at his place: he
        #  is seen to the chest, over a floor, as before)
        m.box((x + 0.12, 0.0, hull.z(-x) - 0.5 * hull.h(-x)), (0.6, 1.0 * hull.w(-x), 0.03), "inside")
    else:
        deck(m, hull, xb, x - 0.6, zf)
        trunk(m, at)
        for knee in legs(m, at, "kneel", zf, spread=0.13):
            m.box((knee.x, knee.y, zf + 0.02), (0.16, 0.13, 0.04), "leather")                     # (his knee pads)
        seat = at(-0.03, 0.0, -HIP_SIT - 0.08)
        m.box(seat, (0.24, 0.2, 0.05), "leather")                                                   # (the bicycle seat he sits back on)
        if seat.z - 0.03 - zf > 0.04:
            m.rod(Vector((seat.x, seat.y, zf)), seat - Z * 0.03, 0.025, "gunmetal", sides=6)
    if f.panes:
        # (her skin is closed round him: he is seen through small framed windows in her sides and a glazed panel over his guns;
        #  the frames are in her skin's own paint: the art director, 2026-11-18, of the hood's pale frame paint here, "reads as toy trim")
        liner(m, f, hull, xb, hull.back, lambda q, high: any(x1 < q < x0 and a0 < high < a1 for x0, x1, a0, a1 in f.panes), inset=0.02)
        cab = s.get("cab")
        for x0, x1, a0, a1 in f.panes:
            wx = [q for q in sorted(f.rings_x, reverse=True) if x1 - 1e-4 <= q <= x0 + 1e-4]
            if cab and x0 <= cab[2] + 1e-3:
                rear_window(m, hull, x0, x1, a0)          # (the flat window across the back of her cabin)
                continue
            arc = [a0 + (a1 - a0) * k / 6.0 for k in range(7)]
            for hand in (1.0, -1.0):
                ang = (lambda a: math.radians(a)) if hand > 0 else (lambda a: math.radians(180.0 - a))
                for a in (a0, a1):
                    if a >= 90.0 and hand < 0:
                        continue                          # (the bar down the middle of a panel across her back, once)
                    if cab and abs(a - cab[6]) < 0.5:
                        a -= 0.7                          # (along the edge of her cabin's roof: on its side, its top edge at the edge)
                    m.strip([port(hull.at(q, ang(a))) for q in wx], [port(hull.out(q, ang(a))) for q in wx], 0.016, "upper", lift=0.006)
                for q in wx:                              # (a bar at each of her frames: small panes)
                    if q > hull.back + 0.02:
                        m.strip([port(hull.at(q, ang(a))) for a in arc], [port(hull.out(q, ang(a))) for a in arc], 0.014 if x1 < q < x0 else 0.018, "upper", lift=0.006)
    for nm, base, tip in f.flex:                         # (his guns' breeches, at his hands)
        if base.x < x - 0.3 and abs(base.y) < 0.3 and m.fine:
            m.use(nm)
            dn = (tip - base).normalized()
            for sy in ((0.0,) if f.name in ONE_GUN else (-1.0, 1.0)):
                m.rod(base - dn * 0.3 + Y * 0.09 * sy, base + dn * 0.04 + Y * 0.09 * sy, 0.04 if sy else 0.055, "gunmetal", sides=6)
            if f.name in ONE_GUN:
                continue
            # (his two guns reach forward inside her tail to his hands: a breech and a spade grip for each, half a metre aft of him;
            #  he faces aft, so his left is to starboard)
            hold = Vector((x - 0.5, 0.0, hull.z(-x) + up - 0.3))
            grips = []
            for sy in (-1.0, 1.0):
                g = hold + Y * 0.11 * sy
                back = base - dn * 0.3 + Y * 0.09 * sy
                to = (back - g).normalized()
                m.rod(g + to * 0.05, g + to * 0.4, 0.047, "gunmetal", sides=8)
                m.rod(g + to * 0.4, back, 0.028, "gunmetal", sides=6)
                m.rod(g - Z * 0.05, g + Z * 0.05, 0.017, "gunmetal", sides=6)
                grips.append(g)
            m.use("tail")
            arms(m, at, grips, fore=nm)
    m.use("plane")


def rear_window(m, hull, xa, xb, a0, half=0.016, bar=0.014, lift=0.005):
    """The flat window across the back of her tail gunner's cabin, between the box's last ring xa and her cone's ring
    xb behind it, from her roof's edge (a0 degrees up from level) across her back: its glass is her skin there
    (pane_at), and laid on it are a frame as thin as her side windows' with its outer edges on the cabin's edges, a bar
    across its middle, and below its sill, as high as theirs, her skin's metal down to her cone, so that it is a
    window with straight sides over her cone (the art director, 2026-11-21: "a frame as thin as theirs, flush at the
    corner, and the middle bar his photo shows")."""
    def at(u, t):
        """A point of the window's face: u its degrees round her from her starboard edge (a0) over her back (90) to her
        port one (180 - a0), t how far from the box's last ring toward her cone's."""
        a = math.radians(u)
        return hull.at(xa, a).lerp(hull.at(xb, a), t)

    def facing(u, t):
        e = 0.2
        du = at(min(u + e, 180.0 - a0), t) - at(max(u - e, a0), t)
        dt = at(u, min(t + 0.02, 1.0)) - at(u, max(t - 0.02, 0.0))
        n = du.cross(dt).normalized()
        return n if n.x < 0.0 else -n                    # (aft)

    def t_of(u, z):
        """How far down the face from the box's ring a height z lies, at u."""
        p1, p0 = hull.at(xa, math.radians(u)), hull.at(xb, math.radians(u))
        return min(max((p1.z - z) / max(p1.z - p0.z, 1e-6), 0.0), 1.0)

    def length(u):
        return (hull.at(xa, math.radians(u)) - hull.at(xb, math.radians(u))).length

    top = hull.at(xa, math.radians(90.0)).z
    sill = hull.at(xa, math.radians(36.0)).z                     # (as high as her side windows' sills)
    us = [a0 + (180.0 - 2.0 * a0) * k / 72.0 for k in range(73)]

    def row(t_at, half_):
        pts = [at(u, t_at(u)) for u in us]
        m.strip([port(p) for p in pts], [port(facing(u, t_at(u))) for u in us], half_, "upper", lift=lift)

    # her skin's metal below the sill, down to her cone and on a little way into it (its face's own slope, under her
    # cone's skin), so that from below and astern no light shows through the glass under its foot
    verts, faces = [], []
    for u in us:
        ts = t_of(u, sill)
        n = facing(u, 0.5 * (ts + 1.0))
        verts += [port(at(u, ts) + n * (lift - 0.002)), port(at(u, 1.0 + 0.04 / max(length(u), 1e-6)) + n * (lift - 0.002))]
    for k in range(len(us) - 1):
        faces.append((2 * k, 2 * k + 2, 2 * k + 3, 2 * k + 1))
    mid_n = port(facing(90.0, 0.5))
    m.add(verts, faces, "upper", False, lambda c: mid_n)
    row(lambda u: half / max(length(u), 1e-6), half)                        # (along her roof's edge)
    row(lambda u: t_of(u, sill), half)                                      # (along its sill)
    row(lambda u: t_of(u, 0.5 * (top + sill)), bar)                         # (the bar across its middle)
    for edge, way in ((a0, 1.0), (180.0 - a0, -1.0)):                       # (up each side, on the cabin's edge)
        ts = [half / max(length(edge), 1e-6) + (t_of(edge, sill) - half / max(length(edge), 1e-6)) * k / 8.0 for k in range(9)]
        pts, ns = [], []
        for t in ts:
            d = (at(edge + 0.2 * way, t) - at(edge, t)).length / 0.2         # (how far a degree is across the face there)
            u = edge + way * half / max(d, 1e-6)
            pts.append(port(at(u, t)))
            ns.append(port(facing(u, t)))
        m.strip(pts, ns, half, "upper", lift=lift)


ONE_GUN = {"Betty"}                                       # (her tail gunner has one cannon, not a pair of guns)
# The machines with a man at each of their blisters: a waist gunner at his gun, or a gunner who sights through it for
# turrets he does not sit in.
BLISTER_CREW = {"Betty", "Nell", "B29"}


def blisters(m, f, s, G, hull):
    """A blister on her side or her back that someone looks out of: the dark of her compartment behind its glass, the
    man's head and shoulders in it, and the breech of his gun if he has one."""
    if f.name not in BLISTER_CREW:
        return
    for c, radii in f.windows:
        if c.z < hull.z(-c.x) or radii[2] < 0.25:
            continue
        m.use("tail" if c.x < f.x_cut else "plane")
        if abs(c.y) < 1e-6:
            figure(m, (c.x, 0.0, c.z + 0.06), 0.0, vest=False, chest=False)
            continue
        hand = 1.0 if c.y > 0.0 else -1.0                # (to port is plus here)

        def skin(x, z, lift=0.0):
            t = max(-0.97, min(0.97, (z - hull.z(-x)) / hull.h(-x)))
            a = math.asin(math.copysign(abs(t) ** (1.0 / hull.power), t))
            return port(hull.at(x, math.pi - a if hand > 0.0 else a, lift))
        rings, around = 4, 20
        pts = [skin(c.x, c.z, 0.012)]
        for i in range(1, rings + 1):
            for k in range(around):
                a = 2.0 * math.pi * k / around
                pts.append(skin(c.x + 0.86 * radii[0] * i / rings * math.cos(a), c.z + 0.86 * radii[2] * i / rings * math.sin(a), 0.012))
        faces = [(0, 1 + k, 1 + (k + 1) % around) for k in range(around)]
        for i in range(1, rings):
            a0, b0 = 1 + (i - 1) * around, 1 + i * around
            faces += [(a0 + k, b0 + k, b0 + (k + 1) % around, a0 + (k + 1) % around) for k in range(around)]
        m.add(pts, faces, "inside", True, lambda q: Vector((0.0, hand, 0.0)))
        at = skin(c.x, c.z + 0.08)
        head = Vector((c.x, at.y + 0.115 * hand, c.z + 0.08))
        face = 90.0 * hand
        for nm, base, tip in f.flex:
            if (base - c).length < 0.8:
                d = tip - base
                face = math.degrees(math.atan2(d.y, d.x))
                head = head + Vector((0.12, 0.0, 0.0))
                if m.fine:
                    m.use(nm)
                    dn = d.normalized()
                    m.rod(base - dn * 0.3, base + dn * 0.04, 0.04, "gunmetal", sides=6)
                    m.use("tail" if c.x < f.x_cut else "plane")
        figure(m, head, face, vest=False, chest=False)
    m.use("plane")


# A second bay whose doors the model does not open: they are scribed in her belly.
SHUT_BAYS = {"B29": [(-2.9, -6.3)]}


def shut_bays(m, f, s, G, hull):
    half = math.radians(f.bay_half)
    lo, mid, hi = 1.5 * math.pi - half, 1.5 * math.pi, 1.5 * math.pi + half
    for x0, x1 in SHUT_BAYS.get(f.name, []) if m.fine else []:
        m.use("tail" if 0.5 * (x0 + x1) < f.x_cut else "plane")
        xs = [x0 + (x1 - x0) * k / 16.0 for k in range(17)]
        for a in (lo, mid, hi):
            m.strip([port(hull.at(x, a)) for x in xs], [port(hull.out(x, a)) for x in xs], 0.006, "line", lift=0.005)
        arc = [lo + (hi - lo) * k / 6.0 for k in range(7)]
        for x in (x0, x1):
            m.strip([port(hull.at(x, a)) for a in arc], [port(hull.out(x, a)) for a in arc], 0.006, "line", lift=0.005)
    m.use("plane")


def jets(m, f, s, G, hull):
    """A jet's engines and her nose: a bright lip and a bullet in each intake, hoops round the pod, a scorched tailpipe
    with its cone; the cannon ports in her nose with their blast troughs, the gun bay's hatches, the nosewheel doors."""
    if not s.get("jets"):
        return
    around = 32 if m.fine else 12
    ring = [2.0 * math.pi * k / around for k in range(around + 1)]
    for pod, out in f.pods:
        m.use(("wing.starboard" if out > 0.0 else "wing.port") if abs(out) > f.y_cut else "plane")
        front = Vector((pod.front, out, pod.z(-pod.front)))
        back = Vector((pod.back, out, pod.z(-pod.back)))
        x = pod.front - 0.035
        m.strip([port(pod.at(x, a)) for a in ring], [port(pod.out(x, a)) for a in ring], 0.035, "steel", lift=0.004)
        m.lathe(port(front) + X * 0.1, -X, [(0.0, 0.0), (0.05, 0.055), (0.17, 0.12), (0.4, 0.165), (0.62, 0.17)], "gunmetal", 16 if m.fine else 8)
        if m.fine:
            for x in (pod.front - 0.5, pod.front - 1.7, pod.front - 2.9):
                m.strip([port(pod.at(x, a)) for a in ring], [port(pod.out(x, a)) for a in ring], 0.005, "line", lift=0.004)
            xs = [pod.front - 0.5 - 2.4 * k / 10.0 for k in range(11)]
            for a in (math.radians(20.0), math.radians(160.0)):
                m.strip([port(pod.at(x, a)) for x in xs], [port(pod.out(x, a)) for x in xs], 0.005, "line", lift=0.004)
        x = pod.back + 0.16
        m.strip([port(pod.at(x, a)) for a in ring], [port(pod.out(x, a)) for a in ring], 0.15, "exhaust", lift=0.004)
        m.lathe(port(back) + X * 0.12, -X, [(0.0, 0.19), (0.2, 0.18), (0.5, 0.07), (0.6, 0.0)], "exhaust", 16 if m.fine else 8, caps=(False, True))
    m.use("plane")
    for (x, deg) in NOSE_CANNON.get(f.name, []):
        a = math.radians(deg)
        xs = [x + 0.02 - 0.34 * k / 5.0 for k in range(6)]
        m.strip([port(hull.at(q, a)) for q in xs], [port(hull.out(q, a)) for q in xs], 0.032, "engine", lift=0.004,
                taper=lambda t: 0.35 + 0.65 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.5)
        f.stains.append((hull, x - 0.3, 1.0, a, 0.5, 0.75))        # (her cannon's smoke, drawn back from each port: fainter than an exhaust's stain, and narrower)
    if m.fine:
        # the gun bay's hatches (hinged along her spine), and the nosewheel's doors under her
        long = [3.9 - 1.6 * k / 10.0 for k in range(11)]
        for deg in (90.0, 22.0, 158.0):
            m.strip([port(hull.at(q, math.radians(deg))) for q in long], [port(hull.out(q, math.radians(deg))) for q in long], 0.005, "line", lift=0.004)
        doors = [3.55 - 1.25 * k / 8.0 for k in range(9)]
        for deg in (258.0, 282.0):
            m.strip([port(hull.at(q, math.radians(deg))) for q in doors], [port(hull.out(q, math.radians(deg))) for q in doors], 0.005, "line", lift=0.004)
        for q in (doors[0], doors[-1]):
            arc = [math.radians(258.0 + 24.0 * k / 4.0) for k in range(5)]
            m.strip([port(hull.at(q, a)) for a in arc], [port(hull.out(q, a)) for a in arc], 0.005, "line", lift=0.004)


# ---- finer work, to Brian's pictures of a B-17 (2026-11-20: "its missing windows where it should be"; "look at all the
#      details and model it after that. with the bomber group markings, windows, gun looks, ect"; "i want the the
#      planes to look very detailed and realistic as possible") ----------------------------------------------------
def wing_part(f, y):
    """The part a thing on a wing belongs to (y in her own axes, to port)."""
    return ("wing.port" if y > 0.0 else "wing.starboard") if abs(y) > f.y_cut else "plane"


def window_frames(m, f, s, G, hull):
    """A frame round each window in her sides (her numbers' `glass_bands`): a narrow band of her own paint standing a
    little proud of her skin over the window's edge, and a bar down the middle of a long one, so that each reads as a
    framed pane of glass and not a dark hole in her."""
    if not s.get("framed_windows") or not m.fine:
        return

    def angle(x, z):
        t = max(-1.0, min(1.0, (z - hull.z(-x)) / hull.h(-x)))
        return math.asin(max(-1.0, min(1.0, math.copysign(abs(t) ** (1.0 / hull.power), t))))
    for x0, x1, half, *high in s.get("glass_bands", []):
        up = high[0] if high else 0.3
        m.use("tail" if 0.5 * (x0 + x1) < f.x_cut else "plane")

        def mid(x, up=up):
            return hull.z(-x) + hull.h(-x) * up
        xs = [x0 + 0.02 + (x1 - x0 - 0.04) * k / 8.0 for k in range(9)]
        bars = [x0, x1] + ([0.5 * (x0 + x1)] if x0 - x1 > 0.45 else [])
        for hand in (1.0, -1.0):
            way = (lambda a: a) if hand > 0.0 else (lambda a: math.pi - a)
            for dz in (half, -half):
                aa = [way(angle(x, mid(x) + dz)) for x in xs]
                m.strip([port(hull.at(x, a)) for x, a in zip(xs, aa)], [port(hull.out(x, a)) for x, a in zip(xs, aa)], 0.024, "upper", lift=0.007)
            for k, x in enumerate(bars):
                zs = [mid(x) - half - 0.024 + (2.0 * half + 0.048) * j / 6.0 for j in range(7)]
                aa = [way(angle(x, z)) for z in zs]
                m.strip([port(hull.at(x, a)) for a in aa], [port(hull.out(x, a)) for a in aa], 0.024 if k < 2 else 0.014, "upper", lift=0.007)
    m.use("plane")


def engine_face(m, f, s, G, hull):
    """The radial in each nacelle as it is seen in the cowling's mouth (her numbers' `engine_face`: how many cylinders).
    Her cowling's ring stands forward of the nacelle's dark face, its lip rounded and ahead of the engine, its outside
    in her coat (olive above, grey where her skin turns under) and its inside dark; within it, a star of finned
    cylinders round the dark nose case, each with its finned head, its rocker box and push rod, all in the cowling's
    shade and behind its lip; and before them, on the nose case, the hub of the airscrew, a small dull dome in front
    of its blades. (The art director, 2026-11-20 17:44Z, of five stills at 10 m: the heads stood "ahead of the cowl lip
    as big grey hex nuts, and the hub is a long white bullet. The cylinders sit inside the cowl ring, finned and dark,
    behind the lip; the hub is a small dull dome in front of the blades.")"""
    n = s.get("engine_face")
    if not n or not m.fine:
        return
    low = s.get("under_from", -0.3)
    turn = math.asin(min(max(-low, -1.0), 1.0))               # (where her coat turns grey: her skin's facing as low as `under_from`)
    outside = [(1.58, 0.668), (1.70, 0.663), (1.79, 0.651), (1.85, 0.637), (1.88, 0.623), (1.897, 0.603)]
    inside = [(1.897, 0.603), (1.893, 0.58), (1.878, 0.563), (1.84, 0.555), (1.62, 0.55)]
    for index, (out, lead, z, is_radial, back) in enumerate(s.get("engines", [])):
        if not is_radial:
            continue
        for hand in (1.0, -1.0):
            side = out * hand
            m.use(wing_part(f, -side))

            def at(dx, r, a):
                return port(Vector((lead + dx, side + math.cos(a) * r, z + math.sin(a) * r)))
            for a0, a1, paint, steps in ((-turn, math.pi + turn, "upper", 30), (math.pi + turn, 2.0 * math.pi - turn, "under", 12)):
                m.skin([[at(dx, r, a0 + (a1 - a0) * k / steps) for k in range(steps + 1)] for dx, r in outside], paint, closed=False)
            m.skin([[at(dx, r, 2.0 * math.pi * k / 32.0) for k in range(32)] for dx, r in inside], "engine", inside=True)
            c = Vector((lead + 1.765, side, z))
            for k in range(n):
                a = math.pi / 2.0 + 2.0 * math.pi * k / n
                d = Vector((0.0, math.cos(a), math.sin(a)))
                m.rod(port(c + d * 0.13), port(c + d * 0.31), 0.055, "engine", sides=8)                  # (the barrel)
                for r in (0.16, 0.195, 0.23, 0.265, 0.3):                                              # (its fins, edge on)
                    m.rod(port(c + d * r), port(c + d * (r + 0.01)), 0.068, "line", sides=8)
                m.rod(port(c + d * 0.31), port(c + d * 0.445), 0.07, "head", sides=8)                  # (the head)
                for r in (0.335, 0.37, 0.405):                                                         # (and its fins)
                    m.rod(port(c + d * r), port(c + d * (r + 0.008)), 0.082, "line", sides=8)
                m.rod(port(c + d * 0.34 + X * 0.05), port(c + d * 0.43 + X * 0.05), 0.028, "engine", sides=6)   # (its rocker box)
                m.rod(port(c + d * 0.14 + X * 0.03), port(c + d * 0.34 + X * 0.045), 0.009, "steel", sides=4)  # (its push rod)
            m.disc(port(c + X * 0.002), X, 0.17, "engine", 16)                                        # (the crankcase)
            m.rod(port(c + X * 0.002), port(Vector((lead + 1.88, side, z))), 0.23, "engine", rb=0.16, sides=16)   # (the nose case and its gears)
            hub = Vector((lead + 1.96, side, z))
            m.rod(port(hub - X * 0.09), port(hub + X * 0.03), 0.15, "hub", sides=16)                  # (the hub, round the blades' roots)
            m.ball(port(hub + X * 0.03), (0.085, 0.15, 0.15), "hub", 16, 8)                           # (its dome)
    m.use("plane")


BLADE = ((0.1, 0.07, 0.05, 62.0), (0.3, 0.115, 0.03, 48.0), (0.6, 0.1, 0.018, 30.0), (0.88, 0.07, 0.01, 20.0), (1.0, 0.03, 0.005, 16.0))     # (the sculptor's blade: share, chord, thickness, twist)


def blade_tips(m, f, s, G, hull):
    """Her blades' yellow tips, a sleeve of paint over the last few inches of each blade; and a plain round disc on each
    blade's face where the maker's badge was (the air thread's brief, 2026-11-20: a real company's mark is not ours to
    copy). Laid on her blades, they turn with them."""
    if not s.get("blade_tips"):
        return
    R, nb = s.get("screw", 1.7), int(s.get("blades", 3))

    def sect(share):
        for (s0, c0, t0, w0), (s1, c1, t1, w1) in zip(BLADE, BLADE[1:]):
            if s0 <= share <= s1:
                k = (share - s0) / (s1 - s0)
                return c0 + (c1 - c0) * k, t0 + (t1 - t0) * k, w0 + (w1 - w0) * k
        return BLADE[-1][1:]

    def frame(hub, angle, share):
        along = Vector((0.0, math.sin(angle), math.cos(angle)))
        chord, thick, twist = sect(share)
        t = math.radians(twist)
        flat = X.cross(along) * math.cos(t) + X * math.sin(t)
        return hub + along * R * share, flat * chord * R, flat.cross(along) * thick * R, along
    for index, (out, lead, z, is_radial, back) in enumerate(s.get("engines", [])):
        for hand in (1.0, -1.0):
            hub = Vector((lead + (1.96 if is_radial else 2.16), out * hand, z))
            k = min(range(len(f.hubs)), key=lambda i: (f.hubs[i] - port(hub)).length)
            m.use(f"prop.{k + 1}")
            turn = 20.0 + (27.0 if is_radial else 31.0) * index
            for b in range(nb):
                angle = math.radians(turn) + 2.0 * math.pi * b / nb
                rings = []
                for share in (0.935, 0.97, 1.0):
                    mid, fl, upv, along = frame(hub, angle, share)
                    rings.append([mid + fl * (math.cos(a) * 1.04) + upv * (math.sin(a) * 1.25) + fl.normalized() * 0.0 for a in (2.0 * math.pi * j / 10.0 for j in range(10))])
                verts = [port(p) for ring in rings for p in ring] + [port(frame(hub, angle, 1.0)[0] + frame(hub, angle, 1.0)[3] * 0.004)]
                faces = [(i * 10 + j, i * 10 + (j + 1) % 10, (i + 1) * 10 + (j + 1) % 10, (i + 1) * 10 + j) for i in range(2) for j in range(10)]
                faces += [(20 + j, 20 + (j + 1) % 10, 30) for j in range(10)]
                axis = [port(hub), port(hub + frame(hub, angle, 1.0)[3] * R * 1.1)]
                m.add(verts, faces, "blade_tip", True, lambda q, a=axis: q - (a[0] + (a[1] - a[0]) * max(0.0, min(1.0, (q - a[0]).dot(a[1] - a[0]) / (a[1] - a[0]).length_squared))))
                if m.fine:
                    mid, fl, upv, along = frame(hub, angle, 0.7)
                    face = upv if port(upv).x > 0.0 else -upv                                        # (on the face she shows ahead)
                    at = mid + face.normalized() * (face.length + 0.003)
                    m.disc(port(at), port(face), 0.05, "decal_rim", 14)
                    m.disc(port(at + face.normalized() * 0.0015), port(face), 0.04, "decal", 14)
    m.use("plane")


def fifties(m, f, s, G, hull):
    """Each of her guns as the heavy machine gun it is: a jacket over the first half of its barrel, its cooling holes
    in dark rings round it, and a flash hider at its muzzle."""
    if not s.get("fifties") or not m.fine:
        return
    for part, a, b in getattr(f, "gun_rods", []):
        d = b - a
        L = d.length
        if L < 0.3:
            continue
        g = d / L
        m.use(part)
        j0, j1 = a + g * (0.16 * L), a + g * (0.56 * L)
        m.rod(j0, j1, 0.033, "gunmetal", sides=8)
        n = max(3, int((j1 - j0).length / 0.075))
        for k in range(n):
            q = j0 + (j1 - j0) * ((k + 0.5) / n)
            m.rod(q - g * 0.011, q + g * 0.011, 0.0345, "engine", sides=8)
        m.rod(b - g * 0.075, b + g * 0.015, 0.025, "gunmetal", sides=8)
        m.disc(b + g * 0.016, g, 0.011, "engine", 8)
    m.use("plane")


def _skin_coat(s):
    """The paint of her skin at a point, by which way it faces, as the sculptor's coat lays it (coat_of, and planes'
    `graded`): her under colour beneath, her second upper colour where her `graded` field says, else her upper."""
    under_from, graded = s.get("under_from", -0.3), s.get("graded")

    def paint(mid, n):
        if n.z < under_from:
            return "under"
        if graded is not None and s.get("patch", 0.0) > 0.0 and graded(port(mid)) > 0.0:
            return "upper2"
        return "upper"
    return paint


def _drop_faces(m, drop):
    """Takes faces out of the model, and the points that only they used."""
    drop = set(drop)
    if not drop:
        return
    gone = {i for k in drop for i in m.faces[k]}
    keep = [k for k in range(len(m.faces)) if k not in drop]
    for name in ("faces", "fpaint", "fpart", "fsmooth", "fcoat"):
        rows = getattr(m, name)
        rows[:] = [rows[k] for k in keep]
    loose = gone - {i for fc in m.faces for i in fc}
    if not loose:
        return
    n = len(m.verts)
    remap, j = {}, 0
    for i in range(n):
        if i not in loose:
            remap[i] = j
            j += 1
    for name in ("verts", "vcoat", "vblend", "vsoot"):
        rows = getattr(m, name)
        rows[:] = [rows[i] for i in range(len(rows)) if i not in loose]
    m.faces[:] = [tuple(remap[i] for i in fc) for fc in m.faces]
    for table in (m.lying, m.uv):
        kept = {remap[i]: v for i, v in table.items() if i in remap}
        table.clear()
        table.update(kept)


def _wheel_in_well(m, hull, x0, z0, rt, rh, squeeze, hand):
    """One main wheel drawn up into the side of her fuselage, as a wheel: its rim laid on her skin as her side view has
    it (the circle squeezed up and down, for the wheel lies in her side, which leans in); the plane of the wheel is
    the plane that rim lies nearest; her skin is cut away round it (`r_cut` from its axis), and the well's dark wall
    runs in from the cut to the well's floor; the tyre lies in the well, its side about flush with her (her skin
    bulges round it, so the middle of the wheel lies a few centimetres in, and the foot of the tyre stands out a
    little where her skin falls away under it), its hub sunk inside it with six bolts."""
    import numpy as np
    from fighters import on_skin
    r_cut, depth, around = rt + 0.016, 0.17, 28
    rim = [port(on_skin(hull, x0 + r_cut * math.cos(a), z0 + r_cut * squeeze * math.sin(a), hand)[0])
           for a in (2.0 * math.pi * k / 36 for k in range(36))]
    P = np.array([tuple(p) for p in rim])
    c = P.mean(axis=0)
    n = Vector(tuple(np.linalg.svd(P - c)[2][2]))
    out = on_skin(hull, x0, z0, hand)[1]
    if n.dot(port(out)) < 0.0:
        n = -n
    C = Vector(tuple(c))
    e1 = (Vector((1.0, 0.0, 0.0)) - n * n.x).normalized()
    e2 = n.cross(e1)

    def ring(r, a, along):
        return C + (e1 * math.cos(a) + e2 * math.sin(a)) * r + n * along

    def radius(p):
        d = p - C
        return (d - n * d.dot(n)).length

    def skin_gap(p):
        """How far outside her skin a point is (its own frame's y is to port: port() turns it to the sculptor's)."""
        q = port(p)
        return abs(q.y) - hull.side_at(q.x, q.z)

    # her skin and what lies on it round the wheel: cut her skin at the well's edge, and take what lies in it away
    plane = m.part_at["plane"]
    skin_paints = {m.paint_at[k] for k in ("upper", "upper2", "under", "nose") if k in m.paint_at}
    near, other = [], []
    for k, fc in enumerate(m.faces):
        if m.fpart[k] != plane:
            continue
        vs = [m.verts[i] for i in fc]
        if any(abs(v.x - x0) > r_cut + 0.12 or v.y * C.y <= 0.0 for v in vs):
            continue
        if any(not -0.2 < (v - C).dot(n) < 0.15 or abs(skin_gap(v)) > 0.02 for v in vs):
            continue
        if min(radius(v) for v in vs) >= r_cut:
            continue
        (near if m.fcoat[k] or m.fpaint[k] in skin_paints else other).append(k)
    first = len(m.verts)
    inside, outside = m.cut_ring(near, C, n, r_cut)
    for k in outside:                                    # (the new points of her skin take her coat from their neighbours)
        fc = m.faces[k]
        for i in fc:
            if i >= first and m.vcoat[i] < 0:
                j = min((q for q in fc if q < first), key=lambda q: (m.verts[q] - m.verts[i]).length, default=None)
                if j is not None:
                    m.vcoat[i], m.vblend[i] = m.vcoat[j], m.vblend[j]
    _drop_faces(m, list(inside) + other)

    # the well: its wall from her skin's edge in to its floor, dark, and the floor behind the wheel
    m.use("plane")
    edge = []
    for k in range(around):
        a = 2.0 * math.pi * k / around
        lo, hi = -0.25, 0.25
        for _ in range(40):
            t = 0.5 * (lo + hi)
            if skin_gap(ring(r_cut, a, t)) > 0.0:
                hi = t
            else:
                lo = t
        edge.append(ring(r_cut, a, 0.5 * (lo + hi) + 0.001))
    floor = [ring(r_cut, 2.0 * math.pi * k / around, -depth) for k in range(around)]
    faces = [(k, (k + 1) % around, around + (k + 1) % around, around + k) for k in range(around)]
    m.add(edge + floor, faces, "engine", False, lambda q: C - q + n * (q - C).dot(n))
    m.add(floor + [C - n * depth], [(k, (k + 1) % around, around) for k in range(around)], "engine", False, lambda q: n)

    # the tyre, from the back of its tread round to the hub, and the hub, dished, with its bolts
    tyre = [(-0.15, rt - 0.03), (-0.11, rt), (-0.05, rt), (-0.025, rt - 0.008), (-0.012, rt - 0.03),
            (-0.008, 0.5 * (rt + rh)), (-0.012, rh + 0.03), (-0.022, rh + 0.006)]
    hub = [(-0.022, rh + 0.006), (-0.03, rh), (-0.042, 0.88 * rh), (-0.048, 0.5 * rh), (-0.04, 0.28 * rh), (-0.03, 0.0)]
    core = 0.5 * (rt + rh)
    for prof, paint, out in ((tyre, "tyre", lambda q: q - (C + n * (-0.06) + (q - C - n * (q - C).dot(n)).normalized() * core)),
                             (hub, "leg", lambda q: n)):
        pts = [ring(max(r, 1e-4), 2.0 * math.pi * k / around, a) for a, r in prof for k in range(around)]
        faces = []
        for j in range(len(prof) - 1):
            for k in range(around):
                k1 = (k + 1) % around
                quad = (j * around + k, j * around + k1, (j + 1) * around + k1, (j + 1) * around + k)
                faces.append(quad if prof[j + 1][1] > 1e-3 else quad[:3])
        m.add(pts, faces, paint, True, out)
    for k in range(6):
        a = math.pi / 2.0 + 2.0 * math.pi * k / 6.0
        m.disc(ring(0.7 * rh, a, -0.042), n, 0.012, "steel", 6)


def side_wheels(m, f, s, G, hull):
    """Main wheels drawn up into wells in the sides of her fuselage, the side of each tyre about flush with her skin
    (the Wildcat's; her numbers' `side_wheels`: (x, z of the wheel's middle as her side view has it, the tyre's
    radius, the hub's, how much the side view squeezes the circle up and down: the wheel lies in her side, which
    leans in)). On the fine model each is a wheel in a well cut into her (_wheel_in_well). On the plain one each is
    laid on her skin point by point: a dark gap round it where the well's edge is, the tyre, and the hub inside it."""
    w = s.get("side_wheels")
    if not w:
        return
    from fighters import on_skin
    x0, z0, rt, rh, squeeze = w
    m.use("plane")
    if m.fine:
        for hand in (1.0, -1.0):
            _wheel_in_well(m, hull, x0, z0, rt, rh, squeeze, hand)
        return
    around = 12
    # (radius out from the middle, how far proud of her skin, paint of the band from this ring in to the next)
    rings = [(rt + 0.016, 0.008, "engine"), (rt, 0.010, "tyre"), (rh, 0.010, "leg"), (0.0, 0.012, "leg")]
    for hand in (1.0, -1.0):
        mid, out = on_skin(hull, x0, z0, hand)
        out = port(out)
        grid = []
        for r, lift, paint in rings:
            row = []
            for k in range(around):
                a = 2.0 * math.pi * k / around
                p, n = on_skin(hull, x0 + r * math.cos(a), z0 + r * squeeze * math.sin(a), hand)
                row.append(port(p + n * lift))
            grid.append(row)
        for i in range(len(rings) - 1):
            if rings[i + 1][0] > 0.0:
                verts = grid[i] + grid[i + 1]
                faces = [(k, (k + 1) % around, around + (k + 1) % around, around + k) for k in range(around)]
            else:
                verts = grid[i] + grid[i + 1][:1]           # (the middle of the hub: a fan round its one point)
                faces = [(k, (k + 1) % around, around) for k in range(around)]
            m.add(verts, faces, rings[i][2], True, lambda q, o=out: o)


def fold(m, f, s, G, hull):
    """The joint of a folding wing (the Wildcat's Sto-Wing; her numbers' `fold`: the joint across each wing as (x, y
    out) from her leading edge to her trailing edge): a fine dark line on her skin over it, on top and beneath. Where
    it runs over her flap, behind the flap's hinge, it is laid on the flap, so that it goes down with it."""
    path = s.get("fold")
    if not path or not m.fine:
        return
    pts = [path[0]]
    for (xa, ya), (xb, yb) in zip(path, path[1:]):
        n = max(1, int(math.ceil(math.hypot(xb - xa, yb - ya) / 0.03)))
        pts += [(xa + (xb - xa) * k / n, ya + (yb - ya) * k / n) for k in range(1, n + 1)]
    wing = G["Wing"](s["wing"])
    flap = s.get("flap", G["BATTLE_FLAP"])
    hinge = next(c for c in G["BATTLE_CHORDS"][True] if c >= flap[2] - 1e-6)      # (as planes cuts the flap: at a ring of hers)
    y0 = max(wing.root + (wing.tip - wing.root) * flap[0], f.y_cut)
    y1 = wing.root + (wing.tip - wing.root) * flap[1]
    for hand in (1.0, -1.0):
        W = G["Wing"](s["wing"], hand)
        own = f"flap.{'port' if hand < 0 else 'starboard'}"

        def share(x, y):
            return (W.lead(y) - x) / (W.lead(y) - W.trail(y))

        def on_flap(x, y):
            return own in m.part_at and y0 < y < y1 and share(x, y) > hinge
        # (the path in runs, each wholly on the wing or wholly on the flap, the two meeting at the hinge)
        runs = [[pts[0]]]
        for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
            if on_flap(xa, ya) != on_flap(xb, yb):
                lo, hi = 0.0, 1.0
                for _ in range(30):
                    mid = 0.5 * (lo + hi)
                    if on_flap(xa + (xb - xa) * mid, ya + (yb - ya) * mid) == on_flap(xa, ya):
                        lo = mid
                    else:
                        hi = mid
                q = (xa + (xb - xa) * 0.5 * (lo + hi), ya + (yb - ya) * 0.5 * (lo + hi))
                runs[-1].append(q)
                runs.append([q])
            runs[-1].append((xb, yb))
        for run in runs:
            if len(run) < 2:
                continue
            mx, my = run[len(run) // 2] if len(run) > 2 else (0.5 * (run[0][0] + run[1][0]), 0.5 * (run[0][1] + run[1][1]))
            m.use(own if on_flap(mx, my) else wing_part(f, -hand * my))
            for upper in (True, False):
                on = []
                for x, y in run:
                    c = min(max(share(x, y), 0.0), 1.0)
                    on.append((port(W.at(y, c, upper)), port(W.out(y, c, upper))))
                m.strip([p for p, n in on], [n for p, n in on], 0.0035, "seam" if upper else "seam_under", lift=0.0015)
    m.use("plane")


def skin_windows(m, f, s, G, hull):
    """Small windows in her skin (her numbers' `skin_windows`: (from, to along her, from, to in height)), each side of
    her: the dark of her inside seen through each, and on the fine model a frame round it in her own paint where it is,
    her sides' or her underside's (fighters' `windows` frames every window in one paint)."""
    wins = s.get("skin_windows")
    if not wins:
        return
    from fighters import on_skin
    paint = _skin_coat(s)
    for x0, x1, z0, z1 in wins:
        m.use("tail" if 0.5 * (x0 + x1) < f.x_cut else "plane")
        xs = [x0 + (x1 - x0) * i / 4.0 for i in range(5)]
        zs = [z0 + (z1 - z0) * j / 3.0 for j in range(4)]
        for hand in (1.0, -1.0):
            grid = [[on_skin(hull, x, z, hand) for z in zs] for x in xs]
            verts = [port(p + n * 0.008) for row in grid for p, n in row]       # (clear of her rivets, 4 mm proud)
            faces = [(i * 4 + j, (i + 1) * 4 + j, (i + 1) * 4 + j + 1, i * 4 + j + 1) for i in range(4) for j in range(3)]
            m.add(verts, faces, "engine", True, lambda q, o=port(grid[2][1][1]): o)
            if m.fine:
                ring = [(x, z0) for x in xs] + [(x1, z) for z in zs[1:]] + [(x, z1) for x in reversed(xs[:-1])] + [(x0, z) for z in reversed(zs[:-1])]
                on = [on_skin(hull, x, z, hand) for x, z in ring]
                m.strip([port(p) for p, n in on], [port(n) for p, n in on], 0.016, paint, lift=0.010)
    m.use("plane")


def skin_rails(m, f, s, G, hull):
    """The rails a sliding hood runs back on, along each side of her (her numbers' `skin_rails`: (from x, at height, to
    x, at height)): a narrow rail in her own paint standing proud of her skin, and the dark of the slot under it."""
    rails = s.get("skin_rails")
    if not rails or not m.fine:
        return
    from fighters import on_skin
    paint = _skin_coat(s)
    for xa, za, xb, zb in rails:
        m.use("tail" if 0.5 * (xa + xb) < f.x_cut else "plane")
        steps = max(2, int(math.ceil(abs(xa - xb) / 0.06)))
        line = [(xa + (xb - xa) * k / steps, za + (zb - za) * k / steps) for k in range(steps + 1)]
        for hand in (1.0, -1.0):
            for dz, half, lift, own in ((-0.014, 0.005, 0.002, "line"), (0.0, 0.009, 0.008, paint)):
                on = [on_skin(hull, x, z + dz, hand) for x, z in line]
                m.strip([port(p) for p, n in on], [port(n) for p, n in on], half, own, lift=lift)
    m.use("plane")


def fit(m, f, s, G, hull):
    CREW.update(nation=nation(s), plane=f.name, n=0)
    bomb_bay(m, f, s, G, hull)
    cockpit(m, f, s, G, hull)
    rear_gunner(m, f, s, G, hull)
    glazed_nose(m, f, s, G, hull)
    gunners(m, f)
    ball_turret(m, f)
    waist(m, f, s, G, hull)
    blisters(m, f, s, G, hull)
    shut_bays(m, f, s, G, hull)
    tail_gunner(m, f, s, G, hull)
    exhausts(m, f, s, G, hull)
    guns(m, f, s, G, hull)
    jets(m, f, s, G, hull)
    fasteners(m, f, s, G, hull)
    rivets(m, f, s, G, hull)
    corrugation(m, f, s, G, hull)
    holed_flaps(m, f, s, G, hull)
    dive_brakes(m, f, s, G, hull)
    boots(m, f, s, G, hull)
    lamps(m, f, s, G, hull)
    window_frames(m, f, s, G, hull)
    engine_face(m, f, s, G, hull)
    blade_tips(m, f, s, G, hull)
    fifties(m, f, s, G, hull)
    side_wheels(m, f, s, G, hull)                           # (each of these four is for a machine whose numbers ask for it:
    fold(m, f, s, G, hull)                                  #  the Wildcat's, carrier_wildcat.py)
    skin_windows(m, f, s, G, hull)
    skin_rails(m, f, s, G, hull)
