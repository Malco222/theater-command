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

from mathutils import Matrix, Vector

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# The paints of the fit-out (added after the game's own). `suit` and `vest` are set by her nation.
PAINTS = {"inside": "39423A", "leather": "3A2B20", "skin": "C59B7C", "suit": "5E5A46", "vest": "5E5A46", "goggles": "17181A", "exhaust": "4E3F36",
          "boot": "1B1B1D", "lamp_red": "C8201A", "lamp_green": "1FA35A", "lamp_white": "F0EBD8", "gunmetal": "26272A", "patch": "A32A22", "lens": "A9C2CC"}
# (flying suit, life jacket: the jacket is worn over the suit, and is yellow where it was)
SUITS = {"UK": ("4D5666", "C9A227"), "GER": ("5C6670", "A39A5C"), "USA": ("6E6247", "C9A227"), "JAP": ("5A4632", "6B5A3C"), "SOV": ("3F3B33", "4A4538"),
         "*": ("5E5A46", "6A6650")}
# Which machines carry guns over the engine, firing through the airscrew.
NOSE_GUNS = {"Bf109", "Zero", "Fw190", "Yak", "Fighter", "Dauntless", "Val"}
# Cannon in a jet's nose, each a port in her skin: (how far along her, where round her in degrees, 90 being her top).
NOSE_CANNON = {"Me262": [(3.78, 66.0), (3.78, 114.0), (3.62, 9.0), (3.62, 171.0)]}
# Who sits under each hood where it is not one pilot: (which hood, how far forward, how far to port, which way he looks).
SEATS = {"Kate": [(0, 0.85, 0.0, 0.0), (0, -0.45, 0.0, 0.0), (0, -1.75, 0.0, 180.0)], "Val": [(0, 0.75, 0.0, 0.0), (0, -1.4, 0.0, 180.0)],
         "Dauntless": [(0, 0.7, 0.0, 0.0), (0, -1.5, 0.0, 180.0)], "Avenger": [(0, 1.5, 0.0, 0.0), (0, -0.6, 0.0, 0.0)],
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


def port(v):
    return Vector((v[0], -v[1], v[2]))


def nation(s):
    tag = (s.get("side_marks") or [(0, 0, "*")])[0][2]
    return tag.split("-")[0]


def paints_for(s):
    out = dict(PAINTS)
    out["suit"], out["vest"] = SUITS.get(nation(s), SUITS["*"])
    return out


def rivet_paints(paints):
    """A rivet is a shade under the paint it sits in, and the joint of two panels a shade under that. On a black coat
    nothing is darker than the paint, so there they are a shade over it: worn edges and dust, as on a night machine."""
    from rigid import mix_hex

    def shade(c, k):
        r, g, b = (int(c[i:i + 2], 16) for i in (0, 2, 4))
        if 0.2126 * r + 0.7152 * g + 0.0722 * b < 44.0:
            return mix_hex(c, "A9A9A3", k * 0.6)
        return mix_hex(c, "000000", k)
    return {"rivet": shade(paints["upper"], 0.3), "rivet_under": shade(paints["under"], 0.24),
            "seam": shade(paints["upper"], 0.36), "seam_under": shade(paints["under"], 0.3)}


def nacelle_of(G, lead, z, is_radial, back, side):
    """An engine's nacelle on a wing, as the game's sculptor shapes it: round and stubby for an air-cooled engine, long
    and narrow for one in line."""
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


def hull_body(Body, s):
    """Her fuselage's body. Round, as the game's are; or, with `taper` in her numbers, a rounded triangle: widest low
    down and drawn in toward her spine, as the Me 262's is (her wheels lie flat in the bottom of it). `taper` is (how
    much of her width is left at her spine, the height the narrowing begins at (-1 her keel, 0 her middle, 1 her
    spine), then stations (x, how much of the taper she has there: none at a round nose, all of it amidships)).
    Her stations' half widths are still her widest, as a plan view shows them. Round her, a point is still named by
    the same angle (its height is what the angle gives): `round_of` gives it back for a point of her skin."""
    if not s.get("taper"):
        return Body(s["hull"], s.get("square", 2.0))
    top, low, along = s["taper"]
    along = sorted(along, key=lambda r: -r[0])
    widest = {}

    class Tapered(Body):
        def share(self, x):
            if x >= along[0][0]:
                return along[0][1]
            for (xa, ka), (xb, kb) in zip(along, along[1:]):
                if x >= xb:
                    return ka + (kb - ka) * (xa - x) / (xa - xb)
            return along[-1][1]

        def narrow(self, k, t):
            return 1.0 - (1.0 - top) * k * max(0.0, (t - low) / (1.0 - low))

        def widest(self, k):
            key = round(k, 3)
            if key not in widest:
                n = 2.0 / self.power
                widest[key] = max((1.0 - abs(t) ** n) ** (1.0 / n) * self.narrow(k, t) for t in (-1.0 + j / 60.0 for j in range(121)))
            return widest[key]

        def at(self, x, a, proud=0.0):
            c, sn = math.cos(a), math.sin(a)
            y, z = math.copysign(abs(c) ** self.power, c), math.copysign(abs(sn) ** self.power, sn)
            k = self.share(x)
            g = self.narrow(k, z) / self.widest(k)
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
            n, k = 2.0 / self.power, self.share(x)
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


def figure(m, head, face=0.0, vest=True, mask=True, chest=True):
    """An airman as he is seen through glass: his head in its helmet and goggles, his shoulders and his chest. `head`
    is the middle of his head; `face` the way he looks, in degrees to port of ahead. Returns a function that gives a
    place beside him: (ahead of his head, to his left, above it)."""
    head = Vector(head)
    rot = Matrix.Rotation(math.radians(face), 3, "Z")
    across = abs(math.sin(math.radians(face))) > 0.7          # (he sits across the machine)

    def at(dx, dy, dz):
        return head + rot @ Vector((dx, dy, dz))

    def r(a, b, c):
        return (b, a, c) if across else (a, b, c)
    u, v = (10, 6) if m.fine else (6, 4)
    m.ball(at(0, 0, 0), r(0.105, 0.098, 0.118), "leather", u, v)
    m.ball(at(0.05, 0, -0.012), r(0.072, 0.076, 0.09), "skin", u, v)
    if chest:
        m.ball(at(-0.02, 0, -0.33), r(0.15, 0.235, 0.2), "suit", u, v)
    else:
        m.ball(at(-0.02, 0, -0.24), r(0.11, 0.2, 0.12), "suit", u, v)                     # (only his shoulders, where there is no room for more of him)
    if vest and chest:
        m.ball(at(0.075, 0, -0.31), r(0.09, 0.165, 0.15), "vest", u, v)
    if m.fine:
        for sy in (-1.0, 1.0):
            m.ball(at(0.104, 0.037 * sy, 0.022), 0.033, "goggles", 8, 4)
            m.ball(at(0.0, 0.1 * sy, -0.005), r(0.045, 0.03, 0.05), "gunmetal", 8, 4)       # his earphones
        if mask:
            m.ball(at(0.1, 0, -0.05), r(0.045, 0.05, 0.048), "inside", 8, 4)
    return at


def arms(m, at, grips, fore=None):
    """A gunner's two arms, from his shoulders to a grip for each hand (his left first; places on the machine): the
    upper arm, the elbow, the forearm and a gloved hand closed on the grip (Brian, 2026-11-19: "he should be in the
    turrett with 2 hands on the 50 call guns"). `fore`: the part his forearms and hands belong to, so that they stay
    on a gun that moves; his upper arms stay with him."""
    body = m.parts[m.cur].name
    u, v = (8, 5) if m.fine else (6, 3)
    for sy, grip in zip((1.0, -1.0), grips):
        grip = Vector(grip)
        sh = at(-0.02, 0.2 * sy, -0.25)
        out = (at(0.0, sy, 0.0) - at(0.0, 0.0, 0.0)).normalized()
        reach = (grip - sh).length
        sag = math.sqrt(max(0.29 ** 2 - (0.5 * reach) ** 2, 0.0))      # (an arm is two bones of 29 cm: the elbow hangs as far off the line as they allow)
        elbow = (sh + grip) * 0.5 + (out * 0.55 - Z * 0.83).normalized() * sag
        m.use(body)
        m.rod(sh, elbow, 0.06, "suit", 0.05, sides=7)
        m.ball(elbow, 0.052, "suit", u, v)
        if fore:
            m.use(fore)
        m.rod(elbow, grip, 0.048, "suit", 0.04, sides=7)
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
            open_cockpit(m, f, s, hull)                    # (her cockpit is open under her canopy: its own dark, and what is in it)
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
        if single and first and face == 0.0 and not getattr(f, "cockpit", None):
            sill = hull.at(x + 0.56, math.pi / 2.0).z
            if x + 0.75 < hood.front:
                wide = min(0.85 * hood.w(-(x + 0.56)), 0.25)
                m.ball((x + 0.56, 0.0, sill), (0.16, min(wide, 0.2), 0.075), "gunmetal", 12, 6)   # the coaming over his instruments
                m.box((x + 0.43, 0.0, z - 0.03), (0.06, 0.05, 0.08), "gunmetal")            # his sight, and its glass
                m.box((x + 0.46, 0.0, z + 0.04), (0.008, 0.075, 0.085), "lens", pitch=-32.0)
        first = False


def open_cockpit(m, f, s, hull):
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
    if m.fine:
        sx = x0 + 0.1
        top = hull.at(sx, math.pi / 2.0).z
        m.box((sx, 0.0, top + 0.045), (0.08, 0.055, 0.09), "gunmetal")
        m.box((sx + 0.02, 0.0, top + 0.12), (0.008, 0.075, 0.085), "lens", pitch=-32.0)
    # (the rim round its edge, padded)
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
    m.box(at(0.06, 0.0, -0.8), (0.42, 0.42, 0.05), "inside")                                                      # his seat
    u, v = (8, 5) if m.fine else (6, 3)
    for sy in (1.0, -1.0):
        hip, knee = at(-0.02, 0.11 * sy, -0.72), at(0.38, 0.13 * sy, -0.62)
        m.rod(hip, knee, 0.075, "suit", 0.062, sides=7)                                                          # his thighs
        m.ball(knee, 0.064, "suit", u, v)
        m.rod(knee, at(0.62, 0.14 * sy, -0.98), 0.058, "suit", 0.05, sides=7)                                     # his shins, to the rudder pedals


def glazed_nose(m, f, s, G, hull):
    """A bomber's glass nose: a bulkhead behind it and a floor in it, so that nobody looks through her, and the men seen in it."""
    glazed = s.get("glazed")
    if not glazed:
        return
    m.use("plane")
    xb = glazed[1] - 0.03
    m.disc((xb, 0.0, hull.z(-xb)), X, hull.h(-xb) * 0.97, "inside", 24 if m.fine else 12, oval=hull.w(-xb) / hull.h(-xb))
    x1 = hull.front - 0.45
    if x1 > glazed[1] + 0.1:
        xm = 0.5 * (x1 + glazed[1])
        m.box((xm, 0.0, hull.z(-xm) - 0.5 * hull.h(-xm)), (x1 - glazed[1], 1.3 * hull.w(-x1), 0.04), "inside")
    for x, y, high, face in NOSE_CREW.get(f.name, []):
        z = body_top(hull, x, -y) - 0.2 if high is None else hull.z(-x) + high
        at = figure(m, (x, y, z), face, vest=False)
        if m.fine and high is None:
            m.box(at(-0.2, 0, -0.3), (0.05, 0.42, 0.36), "inside")


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
        at = figure(m, c + Z * lift - way * 0.1, face, vest=False)
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
                m.rod(grip - over * 0.05, grip + over * 0.05, 0.017, "gunmetal", sides=6)
                grips.append(grip)
        if len(grips) == 2:
            left = at(0.0, 1.0, 0.0) - at(0.0, 0.0, 0.0)
            grips.sort(key=lambda g: -(g - c).dot(left))
            m.use(f"turret.{k}")
            arms(m, at, grips, fore=f"turret.{k}.guns")
    m.use("plane")


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
        m.ball(head, 0.1, "leather", u, v)
        m.ball(head + dw * 0.05 - up * 0.012, 0.076, "skin", u, v)
        m.ball(c - dw * 0.08 + up * 0.02, 0.3, "suit", u, v)
        for hand in (-1.0, 1.0):
            m.ball(c + dw * 0.17 + side * 0.25 * hand + up * 0.1, 0.1, "suit", u, v)
            if m.fine:
                m.ball(head + dw * 0.1 + side * 0.037 * hand + up * 0.02, 0.033, "goggles", 8, 4)
                grip = head + dw * 0.17 + side * 0.105 * hand + up * 0.1      # (a hand on the grip of each gun, up beside his face at the window)
                m.rod(c + dw * 0.17 + side * 0.25 * hand + up * 0.1, grip, 0.045, "suit", 0.038, sides=6)
                m.ball(grip, 0.045, "leather", 8, 5)
        if m.fine:
            m.rod(head + dw * 0.17 - side * 0.16 + up * 0.1, head + dw * 0.17 + side * 0.16 + up * 0.1, 0.012, "gunmetal", sides=5)
        if m.fine:
            m.ball(head + dw * 0.1 - up * 0.05, 0.046, "inside", 8, 4)
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

    def stub(p, n, part, long=0.13, r=0.046):
        tip = p + n * 0.075 + Vector((-long, 0.0, -0.01))
        m.use(part)
        m.rod(port(p - n * 0.02), port(tip), r, "exhaust", r * 0.88, sides=sides)
        way = (tip - (p - n * 0.02)).normalized()
        m.disc(port(tip + way * 0.002), port(way), r * 0.66, "engine", sides)

    def row(body, x0, x1, angle, part):
        for hand in (1.0, -1.0):
            a = math.radians(angle) if hand > 0.0 else math.pi - math.radians(angle)
            for k in range(6):
                x = x0 + (x1 - x0) * k / 5.0
                stub(body.at(x, a), body.out(x, a), part)
            f.stains.append((body, min(x0, x1) - 0.05, 2.6, a))

    def pipes(body, x, part, long=0.4):
        for a in (math.radians(-28.0), math.radians(208.0)):
            stub(body.at(x, a), body.out(x, a), part, long, 0.05)
            f.stains.append((body, x - long, 2.4, a))

    if not s.get("engines"):
        if s.get("stubs"):
            row(hull, s["stubs"][0], s["stubs"][1], s["stubs"][2], "plane")
        elif s.get("engine") == "radial" and s.get("nose_back") is not None:
            pipes(hull, s["nose_back"] - 0.02, "plane")
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            part = ("wing.starboard" if hand > 0.0 else "wing.port") if out > f.y_cut else "plane"
            side = out * hand
            nacelle = nacelle_of(G, lead, z, is_radial, back, side)
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
    if f.name in NOSE_GUNS:
        hub_x = f.hubs[0].x
        m.use("plane")
        for sy in (1.0, -1.0):
            a = math.radians(90.0 + 17.0 * sy)
            xs = [hub_x - 0.5 - 0.85 * k / 6.0 for k in range(7)]
            pts, fac = [port(hull.at(x, a)) for x in xs], [port(hull.out(x, a)) for x in xs]
            m.strip(pts, fac, 0.04, "engine", lift=0.006)
            m.rod(pts[-1] + fac[-1] * 0.028, pts[1] + fac[1] * 0.028, 0.02, "gunmetal", sides=6)


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
    """Her navigation lamps: red to port, green to starboard, white astern."""
    for hand, nm, paint in ((-1.0, "port", "lamp_red"), (1.0, "starboard", "lamp_green")):
        W = G["Wing"](s["wing"], hand)
        u = W.tip - max(0.07, 0.012 * W.tip)
        p = 0.5 * (W.at(u, 0.1, True) + W.at(u, 0.1, False))
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
HOLED = {"Dauntless": (0.95, 3.1, (0.8, 0.86, 0.92))}


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
    r, gap = (0.009, 0.11) if big else (0.007, 0.08)
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

    def dot(part, paint, p, n, t):
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

    def skin_at(x, a):
        """A point of her fuselage's skin, the way it faces and whether it is her under surface; None where neither a
        rivet nor a joint belongs."""
        deg = math.degrees(a) % 360.0
        if 38.0 < deg < 142.0 and any(lo <= x <= hi for hi, lo in hoods):
            return None                                    # (under her hood)
        if f.bay and f.bay[1] - 0.06 <= x <= f.bay[0] + 0.06 and abs(deg - 270.0) < f.bay_half + 2.5:
            return None                                    # (her bomb doors)
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
    for deg in (38.0, 142.0, -40.0, 220.0):
        a = math.radians(deg + 1.2)
        xs = [x_front - (x_front - x_back) * k / n_seam for k in range(n_seam + 1)]
        joint("plane", [skin_at(x, a) for x in xs if x >= f.x_cut - 0.13])
        joint("tail", [skin_at(x, a) for x in xs if x <= f.x_cut + 0.13])

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
            nacelle = nacelle_of(G, lead, z, is_radial, back, out * hand)
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
    def rows(sections, kind, c_max, spars, rib_gap, upright=False, side=0.0, joints=()):
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

            def part_of(u):
                if kind != "wing":
                    return "tail"
                return ("wing.starboard" if hand > 0.0 else "wing.port") if u > f.y_cut else "plane"

            def put(u, c, t_of):
                for upper in (True, False):
                    p, n = W.at(u, c, upper, 0.003), W.out(u, c, upper)
                    dot(part_of(u), "rivet" if (upper or upright) else "rivet_under", p, n, t_of(u, c, upper))
            chordwise = lambda u, c, up: W.at(u, min(c + 0.02, 1.0), up) - W.at(u, max(c - 0.02, 0.0), up)
            spanwise = lambda u, c, up: W.at(min(u + 0.05, W.tip), c, up) - W.at(max(u - 0.05, W.root), c, up)
            n_ribs = max(1, int((u1 - u0) / rib_gap))
            for k in range(n_ribs):
                u = u0 + (u1 - u0) * (k + 0.5) / n_ribs
                n = max(2, int((W.trail(u) - W.lead(u)) * (c_max - 0.05) / gap * (-1.0 if W.trail(u) < W.lead(u) else 1.0)))
                for i in range(n):
                    put(u, 0.05 + (c_max - 0.05) * (i + 0.5) / n, chordwise)
                if k % 2 == 0:                             # (a joint beside every other rib)
                    us = min(u + 0.022, u1)
                    cs = [0.03 + (c_max - 0.03) * i / 10.0 for i in range(11)]
                    for upper in (True, False):
                        joint(part_of(us), [(W.at(us, c, upper, 0.003), W.out(us, c, upper), not (upper or upright)) for c in cs])
            n = max(2, int((u1 - u0) / gap))
            for c in spars:
                for i in range(n):
                    put(u0 + (u1 - u0) * (i + 0.5) / n, c, spanwise)
            cuts = [u0] + ([f.y_cut] if kind == "wing" and u0 < f.y_cut < u1 else []) + [u1]
            for c in joints:                               # (and along her spars)
                for a, b in zip(cuts, cuts[1:]):
                    n = max(2, int((b - a) / 0.3))
                    us = [a + (b - a) * i / n for i in range(n + 1)]
                    for upper in (True, False):
                        joint(part_of(0.5 * (a + b)), [(W.at(u, c + 0.006, upper, 0.003), W.out(u, c + 0.006, upper), not (upper or upright)) for u in us])
    rib = min(max(f.half / 26.0, 0.45), 0.62)              # (a rib every half metre or so)
    rows(s["wing"], "wing", 0.66, (0.1, 0.24, 0.4, 0.56), rib, joints=(0.24, 0.56))
    rows(s["tail"], "tail", 0.56, (0.12, 0.3, 0.48), rib, joints=(0.3,))
    for fin_sections, fin_side in f.fins:
        for side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            rows(fin_sections, "fin", 0.54, (0.12, 0.3, 0.48), rib, True, side, joints=(0.3,))

    # -- laid a part and a paint at a time: each one a small square on its corner, lying in the skin
    for (part, paint), each in dots.items():
        verts, faces = [], []
        for p, n, t in each:
            t = t - n * t.dot(n)
            if t.length < 1e-9:
                continue
            t.normalize()
            w = n.cross(t)
            k = len(verts)
            verts += [p + t * r, p + w * r, p - t * r, p - w * r]
            faces.append((k, k + 1, k + 2, k + 3))
        m.use(part)
        m.add(verts, faces, paint, False)
    m.use("plane")
    f.rivets = sum(len(v) for v in dots.values())


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


def liner(m, f, hull, x_hi, x_lo, open_at, inset=0.035):
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
    m.add(verts, faces, "inside", True, lambda q: Vector((q.x, 0.0, hull.z(-q.x))) - q)
    return xs


def waist(m, f, s, G, hull):
    """A heavy bomber's open waist windows: the dark of the compartment seen through them, a rim round each, and a
    gunner standing at each gun with its breech and its ammunition box."""
    if not f.waist:
        return
    x0, x1, a0, a1 = f.waist
    m.use("tail" if 0.5 * (x0 + x1) < f.x_cut else "plane")
    # the compartment, with the same two windows
    xs = liner(m, f, hull, x0 + 0.62, x1 - 0.62, lambda x, high: x1 < x < x0 and a0 < high < a1)
    for x, way in ((xs[0], -X), (xs[-1], X)):
        m.disc((x, 0.0, hull.z(-x)), way, hull.h(-x) * 0.97, "inside", 24, oval=hull.w(-x) / hull.h(-x))
    xm = 0.5 * (x0 + x1)
    m.box((xm, 0.0, hull.z(-xm) - 0.55 * hull.h(-xm)), (xs[0] - xs[-1], 1.5 * hull.w(-xm), 0.04), "inside")      # (her floor)
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
    m.disc((xb, 0.0, hull.z(-xb)), -X, hull.h(-xb) * 0.96, "inside", 20 if m.fine else 10, oval=hull.w(-xb) / hull.h(-xb))     # (nobody looks along her fuselage)
    m.box((x + 0.12, 0.0, hull.z(-x) - 0.5 * hull.h(-x)), (0.6, 1.0 * hull.w(-x), 0.03), "inside")
    if f.panes:
        # (her skin is closed round him: he is seen through small framed windows in her sides and a glazed panel over his guns;
        #  the frames are in her skin's own paint: the art director, 2026-11-18, of the hood's pale frame paint here, "reads as toy trim")
        liner(m, f, hull, xb, hull.back, lambda q, high: any(x1 < q < x0 and a0 < high < a1 for x0, x1, a0, a1 in f.panes), inset=0.02)
        for x0, x1, a0, a1 in f.panes:
            wx = [q for q in sorted(f.rings_x, reverse=True) if x1 - 1e-4 <= q <= x0 + 1e-4]
            arc = [a0 + (a1 - a0) * k / 6.0 for k in range(7)]
            for hand in (1.0, -1.0):
                ang = (lambda a: math.radians(a)) if hand > 0 else (lambda a: math.radians(180.0 - a))
                for a in (a0, a1):
                    if a >= 90.0 and hand < 0:
                        continue                          # (the bar down the middle of a panel across her back, once)
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


def fit(m, f, s, G, hull):
    bomb_bay(m, f, s, G, hull)
    cockpit(m, f, s, G, hull)
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
    holed_flaps(m, f, s, G, hull)
    boots(m, f, s, G, hull)
    lamps(m, f, s, G, hull)
