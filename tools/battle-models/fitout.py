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
NOSE_CANNON = {"Me262": [(3.72, 68.0), (3.72, 112.0), (3.5, 40.0), (3.5, 140.0)]}
# Who sits under each hood where it is not one pilot: (which hood, how far forward, how far to port, which way he looks).
SEATS = {"Kate": [(0, 0.85, 0.0, 0.0), (0, -0.45, 0.0, 0.0), (0, -1.75, 0.0, 180.0)], "Val": [(0, 0.75, 0.0, 0.0), (0, -1.4, 0.0, 180.0)],
         "Dauntless": [(0, 0.7, 0.0, 0.0), (0, -1.5, 0.0, 180.0)], "Avenger": [(0, 1.5, 0.0, 0.0), (0, -0.6, 0.0, 0.0)],
         "DiveBomber": [(0, 0.7, 0.0, 0.0), (0, -1.3, 0.0, 180.0)], "TorpedoBomber": [(0, 1.15, 0.0, 0.0), (0, -0.3, 0.0, 0.0), (0, -1.7, 0.0, 180.0)],
         "Stuka": [(0, 0.75, 0.0, 0.0), (0, -1.55, 0.0, 180.0)], "Sturmovik": [(0, 0.7, 0.0, 0.0), (0, -1.25, 0.0, 180.0)],
         "He111": [(0, -0.45, 0.0, 180.0)], "Lancaster": [(0, 7.45, 0.3, 0.0), (0, 6.95, -0.3, 0.0), (0, 5.6, 0.0, 0.0)],
         "B17": [(0, 6.05, 0.34, 0.0), (0, 6.05, -0.34, 0.0)], "Bomber": [(0, 3.75, 0.28, 0.0), (0, 3.75, -0.28, 0.0)],
         "Betty": [(0, 5.4, 0.3, 0.0), (0, 5.4, -0.3, 0.0), (0, 3.9, 0.0, 0.0)], "Nell": [(0, 4.45, 0.24, 0.0), (0, 4.45, -0.24, 0.0)]}
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
    """A rivet is a shade under the paint it sits in."""
    from rigid import mix_hex
    return {"rivet": mix_hex(paints["upper"], "000000", 0.3), "rivet_under": mix_hex(paints["under"], "000000", 0.24)}


def body_top(body, x, y=0.0):
    """How high the top of a round body (a fuselage, a hood) stands at x, y off its middle line."""
    w, h = body.w(-x), body.h(-x)
    e = 2.0 / body.power
    t = min(abs(y - body.side) / max(w, 1e-6), 1.0)
    return body.z(-x) + h * (1.0 - t ** e) ** (1.0 / e)


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


def seats_of(f, s, G):
    """Everyone under a hood: (the hood's curves, forward, to port, looks)."""
    hoods = [G["Body"](stations, 2.3) for stations, hoops, spine in s.get("hoods", [])]
    if f.name in SEATS:
        return [(hoods[i], x, y, face) for i, x, y, face in SEATS[f.name] if i < len(hoods)]
    return [(h, h.front - max(0.8, 0.38 * (h.front - h.back)), 0.0, 0.0) for h in hoods[:1]]


def cockpit(m, f, s, G, hull):
    """Under each hood: the dark of the cockpit, and whoever sits there with his seat; a fighter's sight and coaming."""
    m.use("plane")
    for stations, hoops, spine in s.get("hoods", []):
        hood = G["Body"](stations, 2.3)
        n = 14 if m.fine else 6
        xs = [hood.front - 0.12 - (hood.front - hood.back - 0.24) * k / n for k in range(n + 1)]
        half = 0.8 * max(r[2] for r in stations)
        m.strip([port(hull.at(x, math.pi / 2.0)) for x in xs], [port(hull.out(x, math.pi / 2.0)) for x in xs], half, "inside", lift=0.012,
                taper=lambda t: 0.45 + 0.55 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.5)
    single = not s.get("engines")
    first = True
    for hood, x, y, face in seats_of(f, s, G):
        z = body_top(hood, x, -y) - 0.118 - 0.03
        at = figure(m, (x, y, z), face, vest=single)
        if not m.fine:
            continue
        m.box(at(-0.2, 0, -0.3), (0.05, 0.42, 0.36), "inside", yaw=face)                    # the back of his seat
        if single and face == 0.0:
            m.box(at(-0.185, 0, 0.0), (0.02, 0.27, 0.3), "gunmetal")                        # the armour behind his head
        if single and first and face == 0.0:
            sill = hull.at(x + 0.56, math.pi / 2.0).z
            if x + 0.75 < hood.front:
                wide = min(0.85 * hood.w(-(x + 0.56)), 0.25)
                m.ball((x + 0.56, 0.0, sill), (0.16, min(wide, 0.2), 0.075), "gunmetal", 12, 6)   # the coaming over his instruments
                m.box((x + 0.43, 0.0, z - 0.03), (0.06, 0.05, 0.08), "gunmetal")            # his sight, and its glass
                m.box((x + 0.46, 0.0, z + 0.04), (0.008, 0.075, 0.085), "lens", pitch=-32.0)
        first = False


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
    """In each turret: the gunner, looking along his guns, and their breeches beside his head."""
    for k, c, radii in f.turrets:
        if k in f.balls or k in f.remote:
            continue                                      # (see ball_turret; a chin turret is aimed from the nose above it, and a man in it hung out below)
        guns = f.guns_of.get(k, [])
        way = sum(((b - a) for a, b in guns), Vector())
        way.z = 0.0
        face = math.degrees(math.atan2(way.y, way.x)) if way.length > 1e-6 else 0.0
        way = way.normalized() if way.length > 1e-6 else X.copy()
        m.use(f"turret.{k}")
        figure(m, c + Z * (0.1 * radii[2]) - way * 0.06, face, vest=False)
        if m.fine:
            m.use(f"turret.{k}.guns")
            for a, b in guns:
                d = (b - a).normalized()
                m.rod(a - d * 0.3, a + d * 0.04, 0.04, "gunmetal", sides=6)


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
            if is_radial:
                nacelle = G["Body"]([(lead + 1.75, z, 0.46, 0.46), (lead + 1.65, z, 0.66, 0.66), (lead + 0.8, z, 0.68, 0.68), (lead - 0.4, z - 0.05, 0.62, 0.58),
                                     (lead - back * 0.7, z - 0.1, 0.38, 0.32), (lead - back, z - 0.02, 0.08, 0.07)], 2.0, side)
                pipes(nacelle, lead + 0.7, part, 0.5)
            else:
                nacelle = G["Body"]([(lead + 2.55, z, 0.04, 0.04), (lead + 2.1, z, 0.34, 0.34), (lead + 2.05, z - 0.04, 0.4, 0.48), (lead + 0.6, z - 0.1, 0.47, 0.6),
                                     (lead - 1.2, z - 0.14, 0.44, 0.5), (lead - back, z - 0.04, 0.1, 0.1)], 2.3, side)
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
    """Rows of rivets, as small dark dots a shade under her paint: round the fuselage at her frames and along her
    stringers, along the spars and the ribs of her wings, her tailplane and her fin. They are what a smooth skin shows
    when the camera is a wing's length away. (The plain model has none.)"""
    if not m.fine:
        return
    big = bool(s.get("engines"))
    r, gap = (0.011, 0.13) if big else (0.008, 0.085)
    dots = {}

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

    def body_dot(x, a, along):
        deg = math.degrees(a) % 360.0
        if 38.0 < deg < 142.0 and any(lo <= x <= hi for hi, lo in hoods):
            return                                         # (under her hood)
        if f.bay and f.bay[1] - 0.06 <= x <= f.bay[0] + 0.06 and abs(deg - 270.0) < f.bay_half + 2.5:
            return                                         # (her bomb doors)
        p, n = hull.at(x, a, 0.003), hull.out(x, a)
        if f.waist_hole(p, 0.03) or f.pane_at(p, 0.03):
            return                                         # (an open window, a pane of glass)
        for (x0, x1, half_high) in bands:
            if x1 - 0.05 < x < x0 + 0.05 and abs(n.y) > 0.6 and abs(p.z - hull.z(-x) - hull.h(-x) * 0.3) < half_high + 0.04:
                return                                     # (her cabin windows)
        t = X if along else (hull.at(x, a + 0.02) - hull.at(x, a - 0.02))
        dot("tail" if x < f.x_cut else "plane", "rivet_under" if n.z < under_from else "rivet", p, n, t)
    step = max(0.72, f.long / 14.0)
    n_rings = max(2, int((x_front - x_back) / step))
    for k in range(n_rings + 1):
        x = x_front - (x_front - x_back) * k / n_rings
        round_ = 2.0 * math.pi * 0.5 * (hull.w(-x) + hull.h(-x))
        n = max(8, int(round_ / gap))
        for i in range(n):
            body_dot(x, 2.0 * math.pi * (i + 0.5) / n, False)
    n_along = max(4, int((x_front - x_back) / gap))
    for deg in (38.0, 142.0, -24.0, 204.0, 90.0, 270.0):
        for i in range(n_along):
            body_dot(x_front - (x_front - x_back) * (i + 0.5) / n_along, math.radians(deg), True)

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
    def rows(sections, kind, c_max, spars, rib_gap, upright=False, side=0.0):
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
            n = max(2, int((u1 - u0) / gap))
            for c in spars:
                for i in range(n):
                    put(u0 + (u1 - u0) * (i + 0.5) / n, c, spanwise)
    rows(s["wing"], "wing", 0.66, (0.24, 0.56), max(0.5, f.half / 13.0))
    rows(s["tail"], "tail", 0.56, (0.3,), max(0.4, f.half / 20.0))
    for fin_sections, fin_side in f.fins:
        for side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            rows(fin_sections, "fin", 0.54, (0.3,), max(0.4, f.half / 20.0), True, side)

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
        figure(m, head, math.degrees(math.atan2(flat.y, flat.x)), vest=False)
        m.rod(base - flat * 0.34 - Z * 0.02, base - flat * 0.34 - Z * 0.6, 0.03, "gunmetal", sides=6)           # (the post his gun swings on)
        m.box(base - flat * 0.62 + flat.cross(Z) * 0.3 - Z * 0.42, (0.3, 0.16, 0.24), "gunmetal")               # (his ammunition box)
        if m.fine:
            m.use(nm)
            dn = d.normalized()
            m.rod(base - dn * 0.42, base + dn * 0.05, 0.045, "gunmetal", sides=8)                              # (the breech and its grips)
            m.rod(base - dn * 0.42 - Z * 0.07, base - dn * 0.42 + Z * 0.07, 0.016, "leather", sides=5)
    m.use("plane")


# A gunner who sits in her tail behind his windows: (how far along her, how high above her middle).
REAR_CREW = {"B17": (-13.72, 0.11), "B29": (-18.56, 0.12), "Betty": (-10.85, 0.05)}


def tail_gunner(m, f, s, G, hull):
    if f.name not in REAR_CREW:
        return
    x, up = REAR_CREW[f.name]
    m.use("tail")
    figure(m, (x, 0.0, hull.z(-x) + up), 180.0, vest=False)
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
