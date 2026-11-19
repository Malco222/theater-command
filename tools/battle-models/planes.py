"""The aircraft as battle models.

The game already has its aircraft, sculpted by `Tools/build_plane.py` from the numbers in `plane_numbers.py` (copies
of both are in `game/`). Nothing is sculpted again here: each machine is built by that script and then taken apart
into the parts a battle moves:

    plane               the fuselage (the root: the game flies it, the clips only tip and roll it)
    wing.port/starboard each wing outboard of the fuselage, with whatever it carries; breaks away at its root
    tail                the rear fuselage with tailplane, fins and tailwheel; breaks away behind the wing
    prop.N              the blades of each airscrew (numbered from the port side), turning about its hub
    prop.N.blur         the disc a fast-turning airscrew is seen as
    bombs               what hangs under her and can be dropped (where she has it)

    aileron.port/starboard, elevator.port/starboard, rudder (rudder.port/starboard on twin fins), and flap.port/
                        starboard on the single-engined machines: each control surface, hinged where its line is scribed
    turret.N            a gun turret (its dome, its frames, its gunner), training about its ring; turret.N.guns lift
    flexgun.N           a gun on a free mounting (a rear gunner's, a waist gun, a nose gun)

Where a part breaks away both torn ends are closed with the paint `torn`. The paints are the game's own, in its
order, so the game's colours, camouflage patches (blend), soot and wear all carry through; `torn`, `blur` and the
paints of the fit-out (see fitout.py: her crew, her cockpit, her exhausts, her lamps) are added after them.

A battle looks at these machines from a wing's length away, so the game's sculptor is run at a finer grain here
(FINER below: the same curves, more rings), with a ring exactly at each end of every control surface and a line of
points along every hinge, so that the surfaces come away cleanly.

Also here: the parachute and the bomb, small models of their own.

    import planes;  m, facts = planes.BUILDERS["Spitfire"](True)
"""
import math
import os

import bmesh
import bpy
from mathutils import Quaternion, Vector, noise

import fitout
import rigid
from rigid import Model, smooth

HERE = os.path.dirname(os.path.abspath(__file__))
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
_G = None


# Where the rings of a flying surface stand along its chord: close at the nose, and one each side of every hinge line
# (ailerons at 0.74, flaps at 0.76, elevators at 0.62, rudders at 0.60).
CHORDS = [0.0, 0.01, 0.04, 0.09, 0.16, 0.25, 0.36, 0.48, 0.60, 0.62, 0.74, 0.76, 0.86, 0.94, 0.98, 1.0]
CHORDS_PLAIN = [0.0, 0.1, 0.35, 0.62, 0.75, 0.9, 1.0]
BATTLE = {"hull": 0.15, "wing": 0.24, "tail": 0.22, "fin": 0.2}        # (how far apart the rings stand; set for each machine)
FLAP, AILERON, ELEVATOR, RUDDER = (0.1, 0.5, 0.76), (0.54, 0.93, 0.74), (0.08, 0.95, 0.62), (0.06, 1.0, 0.60)
# Where a bomber's bay is: from, to along her (its doors are the strip of her belly BAY_HALF degrees each side of her keel).
BAYS = {"He111": (3.3, 1.3), "Lancaster": (6.4, -3.0), "B17": (1.6, -1.0), "Bomber": (2.0, -0.8)}
BAY_HALF = 27.0
# Open windows in a bomber's sides where a gunner stands at his gun: (from, to along her; from, to in degrees above the
# level of her middle). They are cut on the lines of her skin's own rings, on the fine model only.
WAIST = {"B17": (-4.5, -5.3, 0.0, 27.0)}
# Windows the game's numbers give a machine, made larger where a battle looks in at a man: (from, to, half its height).
GLASS = {"B17": [(-13.1, -14.2, 0.28), (0.85, 0.35, 0.16)]}
LINE = 0.4                                                                # (how wide a scribed line is drawn, as a share of the map's)

# The game's sculptor at a battle's grain: each line of it that sets a grain, and what stands there instead.
FINER = [
    ("around = 28 if fine else 14", "around = 40 if fine else 14"),
    ("hull.xs(0.24 if fine else 0.6)", "hull.xs(BATTLE['hull'] if fine else 0.6)"),
    ("hood_around = 20 if fine else 10", "hood_around = 32 if fine else 10"),
    ("hood.xs(0.2 if fine else 0.5)", "hood.xs(0.1 if fine else 0.5)"),
    ("chords = [0.5 * (1.0 - math.cos(math.pi * k / (9 if fine else 5))) for k in range((9 if fine else 5) + 1)]", "chords = BATTLE_CHORDS[fine]"),
    ("wing.us(0.42 if fine else 1.2)", "battle_us(wing, BATTLE['wing'] if fine else 1.2, s.get('flap', BATTLE_FLAP)[:2] + s.get('aileron', BATTLE_AILERON)[:2])"),
    ("tail.us(0.4 if fine else 1.0)", "battle_us(tail, BATTLE['tail'] if fine else 1.0, BATTLE_ELEVATOR[:2])"),
    ("fin.us(0.3 if fine else 0.8)", "battle_us(fin, BATTLE['fin'] if fine else 0.8, BATTLE_RUDDER[:1])"),
    ("n_around = 20 if fine else 10", "n_around = 28 if fine else 10"),
]


# A battle's own machines, sculpted from numbers in the game's form (the game has no jet). `jets` is new: the pods of
# jet engines under the wings, each (its stations, how far out), open at both ends, with no airscrew anywhere.
EXTRA_PLANES = {
    "Me262": {
        "title": "Me 262A (our own model)", "year": 1944,
        "hull": [(4.45, -0.08, 0.03, 0.03), (4.32, -0.08, 0.11, 0.11), (3.95, -0.07, 0.28, 0.27), (3.3, -0.05, 0.46, 0.44), (2.3, -0.02, 0.63, 0.59), (1.1, 0.02, 0.76, 0.69),
                 (-0.2, 0.05, 0.8, 0.73), (-1.4, 0.09, 0.72, 0.7), (-2.8, 0.18, 0.5, 0.56), (-4.3, 0.3, 0.28, 0.4), (-5.5, 0.4, 0.12, 0.24), (-6.15, 0.45, 0.03, 0.1)],
        "square": 2.6,
        "hoods": [([(0.95, 0.7, 0.22, 0.06), (0.45, 0.9, 0.32, 0.22), (-0.3, 0.96, 0.34, 0.27), (-1.0, 0.92, 0.32, 0.24), (-1.8, 0.72, 0.2, 0.06)], (0.45, -0.3, -1.0), False)],
        "wing": [(0.0, 1.62, -1.42, -0.5, 0.44), (0.9, 1.52, -1.4, -0.48, 0.4), (2.05, 1.05, -1.3, -0.42, 0.34), (5.7, -0.2, -1.45, -0.04, 0.15), (6.15, -0.5, -1.42, 0.0, 0.1),
                 (6.3, -0.82, -1.3, 0.02, 0.03)],
        "tail": [(0.0, -4.7, -5.95, 1.15, 0.11), (1.55, -5.22, -5.95, 1.15, 0.07), (1.88, -5.48, -5.9, 1.15, 0.04), (1.97, -5.7, -5.85, 1.15, 0.01)],
        "fin": [(0, -3.7, -6.1, 0.42, 0.14), (0, -4.55, -6.22, 1.15, 0.12), (0, -5.25, -6.32, 2.0, 0.08), (0, -5.7, -6.3, 2.42, 0.04), (0, -5.92, -6.2, 2.52, 0.01)],
        "patch": 1.7, "colours": {"upper": "4A5240", "upper2": "5F5848", "under": "9DB3BA", "nose": "4A5240", "boss": "2B2B2B"},
        "well": (1.2, 0.42, 0.42), "pitot": True, "hull_lines": [3.3, 2.25, 0.95, -1.85, -3.6],
        "jets": [([(2.42, -0.78, 0.3, 0.3), (2.3, -0.78, 0.36, 0.36), (1.9, -0.78, 0.41, 0.41), (0.4, -0.78, 0.42, 0.42), (-0.7, -0.77, 0.39, 0.39), (-1.35, -0.76, 0.31, 0.31),
                   (-1.62, -0.76, 0.25, 0.25)], 2.05)],
        "side_marks": [(-2.7, 0.33, "GER")], "wing_marks": [(4.3, -0.55, 0.45, "GER", "GER", True, True)], "code": (-1.75, 0.3),
    },
}
JETS = [
    ('    boss_back = 1.0e9 if s.get("engines") else sorted(', '    boss_back = 1.0e9 if s.get("engines") or s.get("jets") else sorted('),
    ('    if not s.get("engines"):\n        airscrew(parts, (hub_x, 0.0, hub_z)', '    if not s.get("engines") and not s.get("jets"):\n        airscrew(parts, (hub_x, 0.0, hub_z)'),
    ('    # The tailwheel, the aerial mast with its wire, and anything fixed under the machine.\n',
     '    for jet_stations, jet_out in s.get("jets", []):\n'
     '        for jet_side in (jet_out, -jet_out):\n'
     '            pod = Body(jet_stations, 2.0, jet_side)\n'
     '            n_pod = 32 if fine else 10\n'
     '            parts.skin("nacelle", [[pod.at(x, 2.0 * math.pi * k / n_pod) for k in range(n_pod)] for x in pod.xs(0.2 if fine else 0.8)],\n'
     '                       with_blend(lambda p, n, front=pod.front, back=pod.back: "engine" if (p.x > front - 0.02 and n.x > 0.5) or (p.x < back + 0.02 and n.x < -0.5) else coat(p, n), coat))\n'
     '    # The tailwheel, the aerial mast with its wire, and anything fixed under the machine.\n'),
]


def battle_us(wing, step, shares=()):
    """Where a flying surface's rings stand along its span: the game's own, closer, and one exactly at each given share
    of the span (the ends of its control surfaces)."""
    us = wing.us(step)
    span = wing.tip - wing.root
    for share in shares:
        u = wing.root + span * share
        us = [v for v in us if abs(v - u) > 0.05 or v in (wing.root, wing.tip)]
        if all(abs(v - u) > 1e-6 for v in us):
            us.append(u)
    return sorted(us)


def game():
    """The game's own aircraft sculptor, loaded without running it, at a battle's grain."""
    global _G
    if _G is None:
        path = os.path.join(HERE, "game", "build_plane.py")
        src = open(path).read().split("\ndef main():")[0]
        for old, new in FINER + JETS:
            if src.count(old) != 1:
                raise ValueError("the game's sculptor has changed here: " + old)
            src = src.replace(old, new)
        _G = {"__file__": path, "__name__": "build_plane", "BATTLE": BATTLE, "BATTLE_CHORDS": {True: CHORDS, False: CHORDS_PLAIN}, "battle_us": battle_us,
              "BATTLE_FLAP": FLAP, "BATTLE_AILERON": AILERON, "BATTLE_ELEVATOR": ELEVATOR, "BATTLE_RUDDER": RUDDER}
        exec(compile(src, path, "exec"), _G)
        _G["PLANES"].update(EXTRA_PLANES)
    return _G


def names():
    return list(game()["PLANES"])


# What kind of machine each is, and how many are aboard (the parachutes a lost one can leave).
CREW = {"Me262": 1, "Bf109": 1, "Spitfire": 1, "Zero": 1, "Mustang": 1, "Yak": 1, "Fw190": 1, "Hurricane": 1, "Warhawk": 1, "Fighter": 1,
        "Stuka": 2, "Sturmovik": 2, "He111": 5, "Lancaster": 7, "B17": 10, "Bomber": 5}
ROLE = {"Stuka": "dive bomber", "Sturmovik": "attack", "He111": "bomber", "Lancaster": "bomber", "B17": "bomber", "Bomber": "bomber"}


def port(v):
    """A place in the game's axes (y to the right) as the model has it (y to port)."""
    return Vector((v[0], -v[1], v[2]))


class Facts:
    """What the clips and the films need to know about a machine."""


def convert(name, fine=True):
    G = game()
    s = G["PLANES"][name]
    if name in GLASS:
        s = dict(s, glass_bands=GLASS[name])
    PAINTS = G["PAINTS"]
    wing, tailw = G["Wing"](s["wing"]), G["Wing"](s["tail"])
    half = wing.tip
    fins = s.get("fins") or [(s["fin"], 0.0)]
    body = G["Body"](s["hull"], s.get("square", 2.0))
    fin_high = max(r[3] for r in fins[0][0]) - min(r[3] for r in fins[0][0])
    BATTLE.update(hull=max(0.15, (body.front - body.back) / 80.0), wing=max(0.24, half / 44.0), tail=max(0.22, tailw.tip / 14.0), fin=max(0.2, fin_high / 12.0))
    parts, hull = G["build"](s, fine)
    colours = G["colours_of"](s)
    paint_names = list(PAINTS)
    paints = {k: colours.get(k, "808080") for k in paint_names}
    paints["torn"] = "2B2825"
    paints["blur"] = "3A3A36"
    paints["line"] = rigid.mix_hex(paints["line"], paints["upper"], 0.3)      # (scribed lines seen close: finer and softer than the map's)
    paints.update(fitout.paints_for(s))
    paints.update(fitout.rivet_paints(paints))
    role = ROLE.get(name, "fighter")
    m = Model(name, s.get("title", name), "aircraft", paints, fine)
    chords = CHORDS if fine else CHORDS_PLAIN

    # ---- where she breaks ------------------------------------------------------------------------
    by_kind = {}
    for ob in parts.objects:
        by_kind.setdefault(ob.name.split(".")[0], []).append(ob)
    root_mid = 0.5 * (wing.lead(wing.root) + wing.trail(wing.root))
    side = hull.w(-root_mid)
    rings_y = sorted({round(abs(v.co.y), 4) for ob in by_kind["wing"] for v in ob.data.vertices})
    y_cut = next((y for y in rings_y if y >= side + 0.12), rings_y[-2])
    rings_x = sorted({round(v.co.x, 4) for ob in by_kind["hull"] for v in ob.data.vertices})
    a, b = wing.trail(wing.root), tailw.lead(tailw.root)
    hood_back = min([min(r[0] for r in h[0]) for h in s.get("hoods", [])] + [a])
    want = min(b + 0.38 * (a - b), hood_back - 0.2)
    ok = [x for x in rings_x if b + 0.15 < x < a - 0.1] or rings_x
    x_cut = min(ok, key=lambda x: abs(x - want))

    # ---- the airscrews ---------------------------------------------------------------------------
    hubs = []
    jets = s.get("jets", [])
    pods = sorted(((G["Body"](st, 2.0, out * hand), out * hand) for st, out in jets for hand in (1.0, -1.0)), key=lambda q: q[1])     # (port first)
    if not s.get("engines") and not jets:
        hub_x = sorted((r[0] for r in s["hull"]), reverse=True)[s.get("spinner", 2) - 1] + 0.06
        hubs.append(Vector((hub_x, 0.0, hull.z(-hub_x))))
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            hubs.append(Vector((lead + (1.96 if is_radial else 2.16), out * hand, z)))
    hubs.sort(key=lambda h: h.y)                       # (the game's y is to the right: port first)
    screw = s.get("screw", 1.5 if not s.get("engines") else 1.7)

    def side_of(y):
        return "wing.port" if y < 0.0 else "wing.starboard"      # (the game's y)

    m.part("plane", None, (0, 0, 0), kind="root", note="the fuselage; the game flies her, the clips only tip and roll her")
    wing_z = wing.z(y_cut)
    for hand, nm in ((-1.0, "wing.port"), (1.0, "wing.starboard")):
        m.part(nm, "plane", port((root_mid, hand * y_cut, wing_z)), kind="free", note="breaks away at the root (fall_spin, fall_explode)")
    m.part("tail", "plane", (x_cut, 0.0, hull.z(-x_cut)), kind="free", note="the rear fuselage and tail; breaks away (fall_explode)")

    def paint_of(ob):
        own = int(ob["paint"])
        return own if own >= 0 or not len(ob.data.polygons) else ob.data.attributes["paint"].data[0].value
    has_bombs = any(paint_of(ob) == PAINTS["bomb"] for ob in by_kind.get("extra", []))
    bay = None
    if name in BAYS:
        near = lambda x: min(rings_x, key=lambda r: abs(r - x))
        bay = (near(BAYS[name][0]), near(BAYS[name][1]))
    if has_bombs or bay:
        m.part("bombs", "plane", (0, 0, 0), kind="slide", slide=(0, 0, -1), slide_limits=(0.0, 60.0), note="what she drops (bombs_away)")
    if bay:
        for nm, a, fore in (("bay.port", 270.0 - BAY_HALF, 1.0), ("bay.starboard", 270.0 + BAY_HALF, -1.0)):
            ends = [port(hull.at(x, math.radians(a))) for x in bay]
            m.part(nm, "plane", 0.5 * (ends[0] + ends[1]), axis=tuple(((ends[0] - ends[1]) * fore).normalized()), kind="turn", limits=(0, 85),
                   note="a door of her bomb bay, hinged along its outer edge; plus opens it")

    around = 40 if fine else 14
    waist = None
    if fine and name in WAIST:
        wx0, wx1, wa0, wa1 = WAIST[name]
        grid = 360.0 / around
        waist = (min(rings_x, key=lambda r: abs(r - wx0)), min(rings_x, key=lambda r: abs(r - wx1)), round(wa0 / grid) * grid, round(wa1 / grid) * grid)

    def waist_hole(c, slack=0.0):
        """Whether a point of her side lies in an open window (either side)."""
        if not waist or not (waist[1] - slack < c.x < waist[0] + slack):
            return False
        a = math.degrees(math.atan2((c.z - hull.z(-c.x)) / hull.h(-c.x), abs(c.y) / hull.w(-c.x)))
        return waist[2] - 40.0 * slack - 0.5 < a < waist[3] + 40.0 * slack + 0.5

    def bay_door(c):
        """The door of her bay a point of her belly is on, if it is (fine model only: the plain one has no keel line to part them at)."""
        if not bay or not fine or not (bay[1] < c.x < bay[0]):
            return None
        a = math.degrees(math.atan2(c.z - hull.z(-c.x), c.y))
        return ("bay.starboard" if c.y > 0.0 else "bay.port") if abs(a + 90.0) < BAY_HALF else None
    prop_parent = []
    for k, h in enumerate(hubs):
        parent = side_of(h.y) if abs(h.y) > y_cut else "plane"
        prop_parent.append(parent)
        m.part(f"prop.{k + 1}", parent, port(h), axis=(1, 0, 0), kind="spin", note="the blades; plus turns them clockwise seen from the cockpit")
        m.part(f"prop.{k + 1}.blur", parent, port(h), axis=(1, 0, 0), kind="spin", note="the disc a fast-turning airscrew is seen as; hidden (scaled away) when it is stopped")

    # ---- the control surfaces --------------------------------------------------------------------
    def cut_at(share):
        return next(c for c in chords if c >= share - 1e-6)
    surf = {}                # its part -> (the skin it is cut from, that skin's curves, from, to, the share of the chord it starts at, its parent, hand)
    span = wing.tip - wing.root
    ail, flap = s.get("aileron", AILERON), s.get("flap", FLAP)
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        W = G["Wing"](s["wing"], hand)
        surf[f"aileron.{nm}"] = ("wing", W, wing.root + span * ail[0], wing.root + span * ail[1], cut_at(ail[2]), f"wing.{nm}", hand)
        if not s.get("engines") and not jets:
            surf[f"flap.{nm}"] = ("wing", W, max(wing.root + span * flap[0], y_cut), wing.root + span * flap[1], cut_at(flap[2]), f"wing.{nm}", hand)
        T = G["Wing"](s["tail"], hand)
        surf[f"elevator.{nm}"] = ("tail", T, T.root + (T.tip - T.root) * ELEVATOR[0], T.root + (T.tip - T.root) * ELEVATOR[1], cut_at(ELEVATOR[2]), "tail", hand)
    for fin_sections, fin_side in fins:
        for hand_side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            F = G["Wing"](fin_sections, 1.0, True, hand_side)
            nm = "rudder" if not fin_side else ("rudder.starboard" if hand_side > 0 else "rudder.port")
            surf[nm] = ("fin", F, F.root + (F.tip - F.root) * RUDDER[0], F.tip + 1e-3, cut_at(RUDDER[2]), "tail", hand_side)
    SWING = {"aileron": (-25, 25), "flap": (0, 45), "elevator": (-28, 22), "rudder": (-30, 30)}
    NOTE = {"aileron": "plus is trailing edge down (a roll to starboard is port aileron plus, starboard minus)", "flap": "plus lowers it",
            "elevator": "plus is trailing edge down (nose down); minus pulls her nose up", "rudder": "plus is trailing edge to starboard (nose to starboard)"}
    for nm, (kind, W, u0, u1, hc, parent, hand) in surf.items():
        ends = []
        for u in (u0, min(u1, W.tip)):
            ends.append(port(0.5 * (W.at(u, hc, True) + W.at(u, hc, False))))
        ax = ends[1] - ends[0]
        if (ax.z < 0.0) if kind == "fin" else (ax.y > 0.0):
            ax = -ax
        m.part(nm, parent, 0.5 * (ends[0] + ends[1]), axis=tuple(ax.normalized()), kind="turn", limits=SWING[nm.split(".")[0]], note=NOTE[nm.split(".")[0]])

    def share(W, u, x):
        u = min(max(u, W.root), W.tip)
        lead, trail = W.lead(u), W.trail(u)
        return (x - lead) / (trail - lead) if abs(trail - lead) > 1e-9 else 0.0

    def on_surface(kind, c):
        """The control surface a face of a flying surface is part of, if it is."""
        for nm, (k, W, u0, u1, hc, parent, hand) in surf.items():
            if k != kind:
                continue
            if kind == "fin":
                if abs(c.y - hand) > 0.5:
                    continue
                u = c.z
            else:
                if (c.y > 0.0) != (hand > 0.0):
                    continue
                u = abs(c.y)
            if u0 + 1e-4 < u < u1 - 1e-4 and share(W, u, c.x) > hc + 1e-4:
                return nm
        return None

    # ---- gun turrets, and guns on free mountings -----------------------------------------------------
    domes = [(Vector(v[0]), Vector(v[1])) for kind, v in s.get("extras", []) if kind == "dome" or (kind == "ball" and v[0][1] == 0.0)]
    rods = []                                           # every gunner's gun, both hands: (its breech, its muzzle) in the game's axes
    for kind, v in s.get("extras", []):
        if kind == "rod" and v[4] == "steel":
            a_, b_ = Vector(v[0]), Vector(v[1])
            if abs(b_.y) > 0.3 and abs(b_.x - a_.x) < 0.2:
                continue
            for hand in ((1.0, -1.0) if (a_.y != 0.0 or b_.y != 0.0) else (1.0,)):
                rods.append((Vector((a_.x, a_.y * hand, a_.z)), Vector((b_.x, b_.y * hand, b_.z))))
    turrets, gun_part = [], {}                           # (number, middle, radii) of each turret; the part of each gun
    for c, radii in domes:
        mine = [i for i, (a_, b_) in enumerate(rods) if (a_ - c).length < max(radii) + 0.5 and i not in gun_part]
        if not mine:
            continue                                     # (a dome with no guns is a window: an astrodome, a bomb-aimer's blister)
        k = len(turrets) + 1
        turrets.append((k, c, radii))
        parent = "tail" if c.x < x_cut else "plane"
        at_end = c.x > hull.front - 1.2 or c.x < hull.back + 1.2
        m.part(f"turret.{k}", parent, port(c), axis=(0, 0, -1), kind="turn", limits=(-75, 75) if at_end else (-180, 180),
               note="a gun turret: its dome, its frames, its gunner; plus trains it clockwise seen from above")
        way = sum((port(rods[i][1] - rods[i][0]) for i in mine), Vector())
        way.z = 0.0
        way = way.normalized() if way.length > 1e-6 else X.copy()
        belly = c.z < hull.z(-c.x) - 0.3 * hull.h(-c.x)
        m.part(f"turret.{k}.guns", f"turret.{k}", port(c), axis=tuple(way.cross(Z).normalized()), kind="turn", limits=(-85, 10) if belly else (-10, 80), note="its guns; plus lifts them")
        for i in mine:
            gun_part[i] = f"turret.{k}.guns"
    flex = []
    for i, (a_, b_) in enumerate(rods):
        if i not in gun_part:
            # (two guns side by side on the middle line are one mounting)
            twin = [j for j, (c_, d_) in enumerate(rods) if j not in gun_part and abs(a_.y) < 0.3 and abs(c_.x - a_.x) < 0.01 and abs(c_.z - a_.z) < 0.01 and abs(abs(c_.y) - abs(a_.y)) < 0.01]
            mine = twin or [i]
            base = sum((rods[j][0] for j in mine), Vector()) / len(mine)
            nm = f"flexgun.{len(flex) + 1}"
            flex.append((nm, base, sum((rods[j][1] for j in mine), Vector()) / len(mine)))
            m.part(nm, "tail" if base.x < x_cut else "plane", port(base), kind="free", note="a gun on a free mounting; it swings about its breech")
            for j in mine:
                gun_part[j] = nm

    def turret_at(p, slack=1.12):
        for k, c, radii in turrets:
            if sum(((p[j] - c[j]) / (radii[j] * slack)) ** 2 for j in range(3)) <= 1.0:
                return f"turret.{k}"
        return None

    def where(kind, c, mean, ob):
        if kind in ("wing", "tail", "fin"):
            on = on_surface(kind, c)
            if on:
                return on
        if kind in ("hull", "lines") and bay_door(c):
            return bay_door(c)
        if kind in ("hull", "lines") and waist_hole(c):
            return None                                     # (an open window: see fitout.waist)
        if kind == "hull":
            return "tail" if c.x < x_cut else "plane"
        if kind == "wing":
            if bay and bay[1] - 0.3 < c.x < bay[0] + 0.3 and abs(c.y) < 0.52 * hull.w(-c.x):
                return None                                 # (the wing runs through her fuselage: what of it would stand in her bay is left out)
            return side_of(c.y) if abs(c.y) > y_cut else "plane"
        if kind in ("tail", "fin", "tailwheel", "tailwheel_leg"):
            return "tail"
        if kind == "blade":
            k = min(range(len(hubs)), key=lambda i: (hubs[i] - Vector((hubs[i].x, mean.y, mean.z))).length)
            return f"prop.{k + 1}"
        if kind == "dome":
            return turret_at(mean, 0.5) or ("tail" if mean.x < x_cut else "plane")
        if kind in ("lines", "frames", "glints"):
            mean = c
            if kind == "frames" and turret_at(c):
                return turret_at(c)
        if kind == "extra":
            if has_bombs and paint_of(ob) == PAINTS["bomb"]:
                return "bombs"
            for i, (a_, b_) in enumerate(rods):
                if (mean - 0.5 * (a_ + b_)).length < 0.03:
                    return gun_part[i]
            if turret_at(mean, 0.3):
                return turret_at(mean, 0.3)
        if kind in ("wire",):
            return "plane"
        if abs(mean.y) > y_cut and mean.x > x_cut:
            return side_of(mean.y)
        if mean.x < x_cut:
            return "tail"
        return "plane"

    # ---- every piece, face by face ---------------------------------------------------------------
    doped = fitout.nation(s) == "UK" and all(g[2] < 0.12 for g in s.get("guns", []))
    for ob in parts.objects:
        kind = ob.name.split(".")[0]
        if (kind == "gun" and doped) or kind == "wire":      # (her gun ports are doped over: see fitout.guns. Brian, 2026-11-18:
            bpy.data.objects.remove(ob, do_unlink=True)      #  "get rid of the wire from the back of the plane to the tail")
            continue
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.verts.ensure_lookup_table()
        if kind == "lines":
            # (every scribed line is a ribbon whose points come in pairs, one each side of it: drawn in to LINE of its width)
            vs = list(bm.verts)
            for i in range(0, len(vs) - 1, 2):
                mid = 0.5 * (vs[i].co + vs[i + 1].co)
                vs[i].co, vs[i + 1].co = mid + (vs[i].co - mid) * LINE, mid + (vs[i + 1].co - mid) * LINE
        bm.faces.ensure_lookup_table()
        layer = bm.faces.layers.int.get("paint")
        by_point = bm.verts.layers.int.get("coat")
        blend_l, soot_l = bm.verts.layers.float.get("blend"), bm.verts.layers.float.get("soot")
        own = int(ob["paint"])
        mean = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
        per_face = kind in ("hull", "wing", "tail", "fin", "lines", "frames", "glints")
        whole = None if per_face else where(kind, mean, mean, ob)
        fpart = [whole or where(kind, f.calc_center_median(), mean, ob) for f in bm.faces]
        loose_ = surf.keys() | {"bay.port", "bay.starboard", None}     # (parts that are cut out clean, not torn off)
        verts, coat, blend, soot, index = [], [], [], [], {}
        out = {}                                             # part -> (faces, paints, by_point)

        def point(v, part):
            key = (v.index, part)
            if key not in index:
                index[key] = len(verts)
                verts.append(port(v.co))
                coat.append(paint_names[v[by_point]] if by_point is not None else None)
                blend.append(v[blend_l] if blend_l is not None else 0.0)
                soot.append(v[soot_l] if soot_l is not None else 0.0)
            return index[key]

        for f, part in zip(bm.faces, fpart):
            if part is None:
                continue
            of_face = own if own >= 0 else f[layer]
            corners = tuple(reversed([point(l.vert, part) for l in f.loops]))      # (mirrored, so turned round)
            faces, fp, fc = out.setdefault(part, ([], [], []))
            faces.append(corners)
            fp.append(paint_names[of_face] if of_face >= 0 else "upper")
            fc.append(of_face < 0)
        # (where a skin is cut between two parts, both torn ends are closed)
        caps = {}
        if kind in ("hull", "wing"):
            for e in bm.edges:
                if len(e.link_faces) == 2:
                    pa, pb = fpart[e.link_faces[0].index], fpart[e.link_faces[1].index]
                    if pa != pb and pa not in loose_ and pb not in loose_:
                        caps.setdefault(tuple(sorted((pa, pb))), []).append(e)
        base = len(m.verts)
        m.verts.extend(verts)
        m.vcoat.extend([m.paint_at[c] if c is not None else -1 for c in coat])
        m.vblend.extend(blend)
        m.vsoot.extend(soot)
        for part, (faces, fp, fc) in out.items():
            pi = m.part_at[part]
            for f, p, c in zip(faces, fp, fc):
                m.faces.append(tuple(base + i for i in f))
                m.fpaint.append(m.paint_at[p])
                m.fpart.append(pi)
                m.fsmooth.append(True)
                m.fcoat.append(c)
        for (pa, pb), edges in caps.items():
            ring = sorted({v.index for e in edges for v in e.verts})
            pts = [port(bm.verts[i].co) for i in ring]
            mid = sum(pts, Vector()) / len(pts)
            # (put the ring in order round its middle: it lies in one plane, across the wing or the fuselage)
            away = Y if kind == "wing" else X
            u = (pts[0] - mid).normalized()
            w = away.cross(u)
            order = sorted(range(len(pts)), key=lambda i: math.atan2((pts[i] - mid).dot(w), (pts[i] - mid).dot(u)))
            ring_pts = [pts[i] for i in order]
            n = len(ring_pts)
            for part, other in ((pa, pb), (pb, pa)):
                def mid_of(p):
                    fs = [f for f, q in zip(bm.faces, fpart) if q == p]
                    return port(sum((f.calc_center_median() for f in fs), Vector()) / len(fs))
                way = mid_of(other) - mid_of(part)
                m.use(part)
                m.add(ring_pts + [mid], [(k, (k + 1) % n, n) for k in range(n)], "torn", False, lambda q, way=way: way)
        bm.free()
        bpy.data.objects.remove(ob, do_unlink=True)

    # ---- the nose of each control surface, and the cove it turns in --------------------------------
    for nm, (kind, W, u0, u1, hc, parent, hand) in surf.items():
        if kind == "wing":
            us = battle_us(W, BATTLE["wing"] if fine else 1.2, tuple(flap[:2]) + tuple(ail[:2]))
        elif kind == "tail":
            us = battle_us(W, BATTLE["tail"] if fine else 1.0, ELEVATOR[:2])
        else:
            us = battle_us(W, BATTLE["fin"] if fine else 0.8, RUDDER[:1])
        us = [u for u in us if u0 - 2e-3 <= u <= u1 + 2e-3]
        if len(us) < 2:
            continue
        n = len(us)
        pts = [port(W.at(u, hc, True)) for u in us] + [port(W.at(u, hc, False)) for u in us]
        quads = [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)]
        m.use(nm)
        m.add(pts, quads, "under", False, lambda q: X)
        m.use(parent)
        m.add(pts, quads, "line", False, lambda q: -X)
        cs = [c for c in chords if c >= hc - 1e-6]
        for u, way in ((us[0], -1.0), (us[-1], 1.0)):
            if kind == "fin" and way > 0.0:
                continue                                  # (a rudder runs to the top of its fin)
            ring = [port(W.at(u, c, True)) for c in cs] + [port(W.at(u, c, False)) for c in reversed(cs[:-1])]
            mid = sum(ring, Vector()) / len(ring)
            away = (Z if kind == "fin" else Vector((0.0, -hand, 0.0))) * way
            fan = [(k, (k + 1) % len(ring), len(ring)) for k in range(len(ring))]
            m.use(nm)
            m.add(ring + [mid], fan, "line", False, lambda q, o=away: o)
            m.use(parent)
            m.add(ring + [mid], fan, "line", False, lambda q, o=away: -o)

    # ---- the disc of each airscrew ---------------------------------------------------------------
    for k, h in enumerate(hubs):
        m.use(f"prop.{k + 1}.blur")
        c = port(h) + X * 0.02
        steps = 28 if fine else 14
        ring = [c + Vector((0.0, math.cos(2 * math.pi * i / steps), math.sin(2 * math.pi * i / steps))) * screw for i in range(steps)]
        m.add(ring + [c], [(i, (i + 1) % steps, steps) for i in range(steps)], "blur", True, lambda q: X)

    # ---- sockets ---------------------------------------------------------------------------------
    for k, h in enumerate(hubs):
        m.socket(f"engine.{k + 1}", port(h) - X * 0.5, X, part=prop_parent[k], note="fire and smoke start here")
    for k, (pod, out) in enumerate(pods):
        part = side_of(out) if abs(out) > y_cut else "plane"
        m.socket(f"engine.{k + 1}", port((pod.front - 1.3, out, pod.z(-pod.front + 1.3))), X, part=part, note="fire and smoke start here")
        m.socket(f"jet.{k + 1}", port((pod.back - 0.3, out, pod.z(-pod.back))), -X, part=part, note="a jet's tailpipe: its heat haze, and at full power its thin dark trail")
    for (x, deg) in fitout.NOSE_CANNON.get(name, []):
        at = hull.at(x, math.radians(deg))
        m.socket(f"gun.{len([k for k in m.sockets if k['name'].startswith('gun.')]) + 1}", port(at) + X * 0.05, X, part="plane", note="a cannon's muzzle in her nose")
    n = 0
    for (out_, back, length, bore) in s.get("guns", []):
        for hand in (-1.0, 1.0):
            w = G["Wing"](s["wing"], hand)
            at = w.at(out_, 0.02, True)
            n += 1
            m.socket(f"gun.{n}", port((at.x + length, at.y, at.z - 0.03)), X, part=side_of(hand) if out_ > y_cut else "plane", note="a fixed gun's muzzle")
    if name in fitout.NOSE_GUNS:
        x = hubs[0].x - 0.5
        top = hull.at(x, math.radians(90.0))
        m.socket("gun.nose", (top.x, 0.0, top.z + 0.03), X, part="plane", note="the guns over the engine")
    for n, (a_, b_) in enumerate(rods):
        m.socket(f"turret.{n + 1}", port(b_), port(b_ - a_), part=gun_part[n], note="a gunner's muzzle")
    belly = hull.at(root_mid if not bay else 0.5 * (bay[0] + bay[1]), -math.pi / 2.0)
    m.socket("bombs", (belly.x, 0.0, belly.z - 0.1), -Z, part="plane", note="where her bombs leave")
    belly = hull.at(root_mid, -math.pi / 2.0)
    hoods = s.get("hoods", [])
    if hoods:
        st = hoods[0][0]
        hx = 0.5 * (max(r[0] for r in st) + min(r[0] for r in st))
        hz = max(r[1] + r[3] for r in st)
        m.socket("hatch", (hx, 0.0, hz + 0.1), Z, part="plane", note="where her crew leave")
    else:
        top = hull.at(root_mid + 1.0, math.pi / 2.0)
        m.socket("hatch", (top.x, 0.0, top.z + 0.1), Z, part="plane", note="where her crew leave")
    m.socket("hatch.belly", (belly.x - 1.2, 0.0, hull.at(belly.x - 1.2, -math.pi / 2.0).z - 0.1), -Z, part="plane", note="a bomber's crew leave by the floor")
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        w = G["Wing"](s["wing"], hand)
        tip = w.at(half * 0.985, 0.5, True)
        m.socket(f"wingtip.{nm}", port(tip), -X, part=f"wing.{nm}", note="a trail from the tip in a hard turn")
        rootp = w.at(y_cut, 0.45, True)
        m.socket(f"break.wing.{nm}", port(rootp), port((0.0, -hand, 0.0)), part=f"wing.{nm}", note="the torn root of the wing: it burns and trails")
        m.socket(f"stub.{nm}", port(rootp), port((0.0, hand, 0.0)), part="plane", note="the stub left on the fuselage: fuel burns here")
        tank = w.at(max(y_cut + 0.6, half * 0.3), 0.4, True)
        m.socket(f"tank.{nm}", port(tank), Z, part=f"wing.{nm}", note="a wing tank: a fire in the wing starts here")
    m.socket("fuel", (root_mid, 0.0, hull.z(-root_mid)), Z, part="plane", note="the middle of her: where she blows up")
    m.socket("break.tail", (x_cut, 0.0, hull.z(-x_cut)), -X, part="tail", note="the torn end of the rear fuselage")
    m.socket("trail", (hull.back, 0.0, hull.z(-hull.back)), -X, part="tail", note="smoke streams from here when she is hit")

    # ---- what the game's file also carries -------------------------------------------------------
    nose_back = s.get("nose_back") if s.get("nose_back") is not None else hull.front - (hull.front - hull.back) * 0.22
    wide = hull.w(0.0)

    def weather(v, n, paint, part):
        w, y = 0.0, abs(v.y)
        if paint not in ("upper", "upper2", "under", "nose"):
            return 0.0, 0.0                                  # (only her paintwork wears)
        if wide * 0.8 < y < wide + 0.9 and n[2] > 0.5 and abs(v.z) < 1.0:
            w = max(w, 0.70 * (1.0 - (y - wide * 0.8) / (0.9 + wide * 0.2)))
        if v.x > nose_back - 0.25:
            w = max(w, 0.38)
        if y > wide and n[0] > 0.75 and y < half:
            w = max(w, 0.45)
        # (seen close she is not one flat colour: the paint is a little uneven all over, the exhausts have drawn a long
        #  stain down each side, and oil has run back along her belly)
        g = Vector((v.x, -v.y, v.z))
        grain = noise.noise(Vector((v.x * 1.3 + 7.1, v.y * 1.3 - 3.3, v.z * 2.1 + 1.7)))
        soot = 0.09 * max(grain, 0.0)
        w = max(w, 0.22 * max(-grain - 0.25, 0.0))
        for stained, start, reach, a in f.stains:
            along = (start - g.x) / reach
            if -0.04 < along < 1.0 and abs(g.y - stained.side) < 2.5:
                away = (g - stained.at(g.x, a)).length
                broad = 0.17 + 0.34 * max(along, 0.0)
                soot = max(soot, 0.95 * max(0.0, 1.0 - away / broad) ** 1.1 * min(1.0, (along + 0.04) * 10.0) * (1.0 - max(along, 0.0)) ** 0.7)
        if not s.get("engines") and not jets and n[2] < -0.5 and y < 0.5 * wide:
            along = (nose_back - v.x) / 4.5
            if 0.0 < along < 1.0:
                soot = max(soot, 0.4 * (1.0 - along) * (1.0 - y / (0.5 * wide)))
        return w, soot
    m.weather = weather
    m.marks = G["marks_of"](s, hull)
    m.info.update({"rust": "9A9C98", "soot": colours["soot"], "wear_show": 0.3,
                   "file": {"nation_paint": bool(s.get("nation_paint")), "number_ink": colours["ink"], "span": round(2.0 * half, 3),
                            "role": role, "crew": CREW.get(name, 1), "engines": len(hubs) + len(pods), "jets": len(pods), "year": s.get("year"),
                            "clear": {"glass": 0.3, "blur": 0.14}}})
    f = Facts()
    f.name, f.spec, f.hull, f.wing, f.half, f.y_cut, f.x_cut, f.hubs, f.screw = name, s, hull, wing, half, y_cut, x_cut, [port(h) for h in hubs], screw
    f.role, f.crew, f.has_bombs, f.long = role, CREW.get(name, 1), has_bombs or bool(bay), hull.front - hull.back
    f.bay, f.bay_half, f.rings_x = (bay if fine else None), BAY_HALF, rings_x
    f.prop_parent = prop_parent
    f.fins = fins
    f.engines, f.pods = len(hubs) + len(pods), pods
    f.waist, f.around, f.waist_hole = waist, around, waist_hole
    f.surf = list(surf)
    f.turrets = [(k, port(c), radii) for k, c, radii in turrets]
    f.flex = [(nm, port(a_), port(b_)) for nm, a_, b_ in flex]
    f.guns_of = {k: [(port(rods[i][0]), port(rods[i][1])) for i in range(len(rods)) if gun_part[i] == f"turret.{k}.guns"] for k, c, radii in turrets}
    fitout.fit(m, f, s, G, hull)
    clips_for_plane(m, f)
    return m, f


# ---- the clips ----------------------------------------------------------------------------------------

def q_of(roll=0.0, pitch=0.0, yaw=0.0):
    """An attitude: yaw to port, then nose up, then a roll to starboard (degrees), in the model's axes."""
    return (Quaternion(Z, math.radians(yaw)) @ Quaternion(Y, math.radians(-pitch)) @ Quaternion(X, math.radians(roll)))


def rot_keys(f, long, step=0.1):
    """Keys of a turning over time: f(t) -> quaternion."""
    n = max(1, int(round(long / step)))
    out = []
    last = None
    for k in range(n + 1):
        t = long * k / n
        q = f(t)
        if last is not None and q.dot(last) < 0.0:
            q = -q
        last = q
        out.append([round(t, 3), q.w, q.x, q.y, q.z])
    return out


def move_keys(f, long, step=0.1):
    n = max(1, int(round(long / step)))
    return [[round(long * k / n, 3), *f(long * k / n)] for k in range(n + 1)]


def loose(m, clip, part, parent_q, world_q, world_move, long, step=0.1):
    """A part that has come away: it is still the fuselage's child, so what it does in the open air (turning as
    world_q(t), drifting as world_move(t) from where it was) is worked back through the fuselage's own turning."""
    P = m.parts[m.part_at[part]]
    root = m.parts[P.parent]

    def local(t):
        qb = parent_q(t)
        inv = qb.inverted()
        q = inv @ world_q(t)
        at = P.pivot + Vector(world_move(t))                              # where its pivot should be, in the root's frame
        mv = inv @ (at - root.pivot) + root.pivot - P.pivot
        return q, mv
    clip["tracks"].append({"part": part, "rot": rot_keys(lambda t: local(t)[0], long, step), "move": move_keys(lambda t: tuple(local(t)[1]), long, step)})


def props(m, f, how):
    """The tracks of the airscrews: 'fast' (the disc alone), 'slow' (blades alone, turning), 'dead' (blades alone, still)."""
    def tracks(long):
        out = []
        for k in range(len(f.hubs)):
            blades, disc = f"prop.{k + 1}", f"prop.{k + 1}.blur"
            if how == "fast":
                out.append({"part": blades, "scale": [[0.0, 0.02]]})
                out.append({"part": disc, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.5))]]})
            elif how == "slow":
                out.append({"part": blades, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.2))]]})
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
            else:
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
        return out
    return tracks


def clips_for_plane(m, f):
    C = m.clips
    eng = [f"engine.{k + 1}" for k in range(f.engines)]
    big = f.half > 9.0
    rudders = [n for n in f.surf if n.startswith("rudder")]

    def stick(roll=None, pitch=None, yaw=None):
        """What her pilot does with the controls, as tracks of her control surfaces. Each is a list of [t, degrees] keys,
        signed as q_of: roll plus is to starboard, pitch plus is nose up, yaw plus is to port."""
        out = []
        if roll:
            out.append({"part": "aileron.port", "turn": [[t, d] for t, d in roll]})
            out.append({"part": "aileron.starboard", "turn": [[t, -d] for t, d in roll]})
        if pitch:
            out += [{"part": nm, "turn": [[t, -d] for t, d in pitch]} for nm in ("elevator.port", "elevator.starboard")]
        if yaw:
            out += [{"part": nm, "turn": [[t, -d] for t, d in yaw]} for nm in rudders]
        return out
    # -- flying --
    long = 4.0
    # (the sound director's real loops, by the kind of machine)
    engine_cue = "jet_engine_loop" if f.pods else ("b17_drone_loop" if f.name == "B17" else "b24_drone_loop" if len(eng) >= 4 else "bomber_medium_loop" if f.role == "bomber"
                  else "stuka_drone_loop" if f.name == "Stuka" else "fighter_engine_loop")       # (no jet is recorded yet: `jet_engine_loop` stays silent)
    C["fly"] = {"long": long, "loop": True, "about": "Under way: every airscrew a turning disc, and she rides the air a little. The game flies her; this only lives on top.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=(0.6 if big else 1.4) * math.sin(2 * math.pi * t / long), pitch=(0.25 if big else 0.5) * math.sin(4 * math.pi * t / long + 0.7)), long, 0.25)}]
                + props(m, f, "fast")(long) + stick(roll=[[0.0, 1.2], [1.0, 0.0], [2.0, -1.2], [3.0, 0.0], [4.0, 1.2]], pitch=[[0.0, 0.6], [1.0, 0.0], [2.0, -0.6], [3.0, 0.0], [4.0, 0.6]]),
                "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": engine_cue}]}
    C["idle"] = {"long": 2.5, "loop": True, "about": "Engines ticking over on the ground or the deck: the blades are seen going round.",
                 "tracks": props(m, f, "slow")(2.5), "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": "aero_engine_idle_loop"}]}
    C["stopped"] = {"long": 0.1, "loop": False, "after": "hold", "about": "Engines stopped: blades still, no disc. For a parked machine.", "tracks": props(m, f, "dead")(0.1), "events": []}
    for nm, sign in (("bank.port", -1.0), ("bank.starboard", 1.0)):
        deg = 28.0 if big else 62.0
        L, over = (1.6, 10.0) if big else (0.9, 15.0)
        C[nm] = {"long": L, "loop": False, "after": "hold", "about": "Rolls into a turn and holds it, her ailerons over as she rolls and her elevators up as she pulls round "
                 "(lay it over `fly`; play it backwards to roll out).",
                 "tracks": [{"part": "plane", "rot": rot_keys(lambda t, s_=sign, d=deg, L=L: q_of(roll=s_ * d * smooth(t / L)), L, 0.1)}]
                 + stick(roll=[[0.0, 0.0], [0.25 * L, over * sign], [0.7 * L, over * sign], [L, 1.5 * sign]], pitch=[[0.0, 0.0], [L, 7.0]],
                         yaw=[[0.0, 0.0], [0.5 * L, -6.0 * sign], [L, -2.0 * sign]]), "events": []}
    if not big:
        C["peel_off"] = {"long": 1.8, "loop": False, "after": "hold", "about": "Half-rolls and pulls down into a dive (the game then flies her down).",
                         "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-150.0 * smooth(t / 1.1), pitch=-55.0 * smooth((t - 0.5) / 1.3)), 1.8, 0.1)}]
                         + stick(roll=[[0.0, 0.0], [0.2, -18.0], [0.9, -18.0], [1.2, 0.0]], pitch=[[0.0, 0.0], [0.5, 0.0], [1.0, 16.0], [1.8, 10.0]]), "events": []}
    if "flap.port" in f.surf:
        C["flaps.down"] = {"long": 1.5, "loop": False, "after": "hold", "about": "Her flaps come down, for a landing or a take-off from a deck (play it backwards to raise them).",
                           "tracks": [{"part": nm, "turn": [[0.0, 0.0], [1.5, 35.0]]} for nm in ("flap.port", "flap.starboard")], "events": []}
    # -- shooting --
    guns = [s_["name"] for s_ in m.sockets if s_["name"].startswith("gun.")]
    if guns:
        C["fire"] = {"long": 0.5, "loop": True, "about": "Her fixed guns firing: a flicker at each muzzle, and she shakes with it.",
                     "tracks": [{"part": "plane", "move": [[0.0, 0, 0, 0], [0.06, -0.012, 0, 0.004], [0.12, 0, 0, 0], [0.18, -0.012, 0, -0.004], [0.25, 0, 0, 0], [0.31, -0.012, 0, 0.004], [0.37, 0, 0, 0], [0.43, -0.012, 0, -0.004], [0.5, 0, 0, 0]]}],
                     "events": [{"t": round(0.08 * k, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "plane_guns" if k == 0 else ""} for k in range(6) for g in guns]}
    turrets = [s_["name"] for s_ in m.sockets if s_["name"].startswith("turret.")]
    if turrets:
        swing = []
        for k, c, radii in f.turrets:
            swing.append({"part": f"turret.{k}", "turn": [[0.0, 0.0], [0.15, 4.0], [0.3, 0.0], [0.45, -4.0], [0.6, 0.0]]})
            swing.append({"part": f"turret.{k}.guns", "turn": [[0.0, 0.0], [0.3, 2.5], [0.6, 0.0]]})
        for nm, a, b in f.flex:
            swing.append({"part": nm, "rot": rot_keys(lambda t: q_of(yaw=4.0 * math.sin(2 * math.pi * t / 0.6), pitch=2.0 * math.sin(4 * math.pi * t / 0.6)), 0.6, 0.075)})
        C["fire.turrets"] = {"long": 0.6, "loop": True, "about": "Her gunners firing back: a flicker at each gunner's muzzle, each turret and gun working a little about its aim "
                             "(the game trains a turret at its target with the part's own axis and limits; this lies on top).", "tracks": swing,
                             "events": [{"t": round(0.1 * k + 0.03 * i, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "gunner_burst" if k == 0 and i == 0 else ""} for k in range(5) for i, g in enumerate(turrets)]}
    if f.turrets or f.flex:
        long = 8.0
        swing = []
        for i, (k, c, radii) in enumerate(f.turrets):
            lim = m.parts[m.part_at[f"turret.{k}"]].limits
            reach = 0.55 * min(lim[1], 80.0)
            swing.append({"part": f"turret.{k}", "turn": [[round(long * j / 16.0, 3), round(reach * math.sin(2 * math.pi * j / 16.0 + 1.3 * i), 2)] for j in range(17)]})
            swing.append({"part": f"turret.{k}.guns", "turn": [[round(long * j / 8.0, 3), round(6.0 + 6.0 * math.sin(2 * math.pi * j / 8.0 + 0.7 * i), 2)] for j in range(9)]})
        for i, (nm, a, b) in enumerate(f.flex):
            swing.append({"part": nm, "rot": rot_keys(lambda t, i=i: q_of(yaw=14.0 * math.sin(2 * math.pi * t / long + 1.1 * i), pitch=6.0 * math.sin(4 * math.pi * t / long + 0.5 * i)), long, 0.25)})
        C["watch"] = {"long": long, "loop": True, "about": "Her gunners keeping watch: every turret and gun sweeping slowly across its piece of sky (lay it over `fly`).",
                      "tracks": swing, "events": []}
    if f.has_bombs or f.role in ("bomber", "dive bomber", "attack"):
        tracks = []
        doors = "bay.port" in m.part_at
        t0 = 0.9 if doors else 0.0                         # (a bay's doors open first)
        if doors:
            tracks += [{"part": nm, "turn": [[0.0, 0.0], [0.8, 80.0]]} for nm in ("bay.port", "bay.starboard")]
        if f.has_bombs:
            tracks.append({"part": "bombs", "slide": [[0.0, 0.0], [t0, 0.0], [t0 + 0.5, 1.2], [t0 + 1.0, 4.9], [t0 + 1.5, 11.0]], "scale": [[0.0, 1.0], [t0 + 1.4, 1.0], [t0 + 1.5, 0.001]]})
        C["bombs_away"] = {"long": t0 + 1.5, "loop": False, "after": "hold", "about": "Her bombs leave: " + ("the doors of her bay swing open, her bombs drop out of it and are gone" if doors else
                           "what hangs under her drops clear and is gone" if f.has_bombs else "from under her") + " (the game's falling bombs are spawned at `bombs` and take over).",
                           "tracks": tracks + [{"part": "plane", "move": [[0.0, 0, 0, 0], [t0, 0, 0, 0], [t0 + 0.4, 0, 0, 0.25 if not big else 0.12], [t0 + 1.5, 0, 0, 0]]}],
                           "events": ([{"t": 0.0, "fx": "", "socket": "bombs", "cue": "bomb_doors"}] if doors else []) + [{"t": t0, "fx": "bombs_away", "socket": "bombs", "cue": "bomb_release"}]}
        if doors:
            C["doors.close"] = {"long": 0.9, "loop": False, "after": "hold", "about": "The doors of her bay swing shut on an empty bay.",
                                "tracks": [{"part": nm, "turn": [[0.0, 80.0], [0.9, 0.0]]} for nm in ("bay.port", "bay.starboard")] + [{"part": "bombs", "scale": [[0.0, 0.001]]}],
                                "events": [{"t": 0.0, "fx": "", "socket": "bombs", "cue": "bomb_doors"}]}
    # -- being hit --
    C["hit"] = {"long": 0.7, "loop": False, "about": "Struck: she staggers and a puff of smoke and scraps flies from her.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=9.0 * math.sin(t * 18.0) * (1 - t / 0.7), pitch=-3.0 * math.sin(t * 9.0) * (1 - t / 0.7)), 0.7, 0.05)}],
                "events": [{"t": 0.0, "fx": "hit_flash", "socket": "fuel", "cue": "ricochet_01"}, {"t": 0.02, "fx": "debris_small", "socket": "fuel", "cue": ""}]}
    C["smoking"] = {"long": 2.0, "loop": True, "about": "Damaged but flying: a thin grey stream from an engine.", "tracks": [],
                    "events": [{"t": 0.0, "fx": "smoke_trail_thin", "socket": eng[-1], "cue": ""}]}
    # -- the three ways down (ruling 217: a loss is one model falling) --
    long = 9.0
    C["fall_burn"] = {"long": long, "loop": False, "after": "hold", "about": "Shot down burning: an engine alight, she drops a wing and goes down in a steepening dive, trailing fire and black smoke. "
                      "The game carries her down; this is her attitude, her fire and her smoke. Play `bail_out` over it if her crew get out.",
                      "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-(18.0 * smooth(t / 2.0) + 50.0 * (t / long) ** 2 * (1.0 if not big else 0.6)), pitch=-(8.0 * smooth(t / 1.5) + 52.0 * smooth((t - 1.0) / 6.0)), yaw=10.0 * smooth(t / long)), long, 0.2)}]
                      + props(m, f, "slow")(long)[:2] + props(m, f, "fast")(long)[2:] + stick(roll=[[0.0, 0.0], [2.0, 9.0], [long, 4.0]], pitch=[[0.0, 0.0], [3.0, 6.0], [long, -8.0]]),
                      "events": [{"t": 0.0, "fx": "engine_fire", "socket": eng[0], "cue": "aircraft_fire"}, {"t": 0.0, "fx": "smoke_trail_black", "socket": eng[0], "cue": ""},
                                 {"t": 2.5, "fx": "fire", "socket": "stub.port" if len(eng) == 1 else "tank.port", "cue": ""}, {"t": 0.0, "fx": "engine_dying", "socket": eng[0], "cue": "plane_down"}]}
    # (a wing gone: she rolls hard toward the missing wing and whirls down; the wing flutters away behind)
    long = 8.0
    spin_q = lambda t: q_of(yaw=-70.0 * t * smooth(t / 1.5)) @ q_of(pitch=-72.0 * smooth(t / 1.8)) @ q_of(roll=-(200.0 if not big else 110.0) * t * smooth(t / 1.0))
    clip = {"long": long, "loop": False, "after": "hold", "about": "A wing shot off: the port wing breaks away at the root, she whirls down out of control, the wing fluttering down behind her. "
            "The game carries her down.",
            "tracks": [{"part": "plane", "rot": rot_keys(spin_q, long, 0.05)}] + props(m, f, "slow")(long)
            + stick(roll=[[0.0, 0.0], [0.5, 20.0]], pitch=[[0.0, 0.0], [0.6, 24.0]], yaw=[[0.0, 0.0], [0.5, -26.0]]),
            "events": [{"t": 0.0, "fx": "hit_flash", "socket": "break.wing.port", "cue": "aircraft_break_up"}, {"t": 0.05, "fx": "fire", "socket": "stub.port", "cue": ""},
                       {"t": 0.05, "fx": "smoke_trail_black", "socket": "stub.port", "cue": "plane_down"}, {"t": 0.1, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""}]}
    drag = (26.0 if not big else 18.0)
    loose(m, clip, "wing.port", spin_q,
          lambda t: q_of(roll=40.0 * t + 260.0 * t * smooth(t / 2.0), pitch=25.0 * math.sin(t * 2.2), yaw=-95.0 * t),
          lambda t: (-drag * t - 3.0 * t * t, 1.5 * t + 4.0 * math.sin(t * 1.3), 5.0 * t + 2.2 * t * t), long, 0.05)
    C["fall_spin"] = clip
    # (blown apart: both wings and the tail fly off; the pieces tumble and fall away from each other)
    long = 6.0
    body_q = lambda t: q_of(pitch=-80.0 * smooth(t / 2.5), roll=140.0 * t, yaw=25.0 * t)
    clip = {"long": long, "loop": False, "after": "gone", "about": "Blown up: a burst of fire amidships, both wings and the tail fly off and the pieces tumble away, burning. "
            "The game carries the wreck down and takes it away at the end.",
            "tracks": [{"part": "plane", "rot": rot_keys(body_q, long, 0.05)}] + props(m, f, "dead")(long),
            "events": [{"t": 0.0, "fx": "explode", "socket": "fuel", "cue": "plane_explode"}, {"t": 0.0, "fx": "debris", "socket": "fuel", "cue": ""},
                       {"t": 0.1, "fx": "fire", "socket": "fuel", "cue": ""}, {"t": 0.1, "fx": "smoke_trail_black", "socket": "fuel", "cue": ""},
                       {"t": 0.15, "fx": "fire", "socket": "tank.starboard", "cue": ""}, {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""},
                       {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.tail", "cue": ""}]}
    k = 1.0 if not big else 0.7
    loose(m, clip, "wing.port", body_q, lambda t: q_of(roll=-330.0 * t * k, pitch=30.0 * t, yaw=-120.0 * t * k),
          lambda t: (-22.0 * t - 2.0 * t * t, 9.0 * t * math.exp(-t / 3.0) + 3.0 * t, 6.0 * t + 2.0 * t * t), long, 0.05)
    loose(m, clip, "wing.starboard", body_q, lambda t: q_of(roll=280.0 * t * k, pitch=-50.0 * t, yaw=150.0 * t * k),
          lambda t: (-17.0 * t - 2.5 * t * t, -11.0 * t * math.exp(-t / 3.0) - 2.0 * t, 3.0 * t + 2.6 * t * t), long, 0.05)
    loose(m, clip, "tail", body_q, lambda t: q_of(pitch=420.0 * t * k, yaw=60.0 * t, roll=45.0 * t),
          lambda t: (-30.0 * t - 1.5 * t * t, 2.0 * t, 9.0 * t + 1.5 * t * t), long, 0.05)
    C["fall_explode"] = clip
    # -- her crew --
    crew = min(f.crew, 5)
    C["bail_out"] = {"long": 0.8 + 0.7 * crew, "loop": False, "about": f"Her crew take to their parachutes: {crew} leave one after another (the game spawns the Parachute model at each `chute` event).",
                     "tracks": [], "events": [{"t": round(0.5 + 0.7 * k, 2), "fx": "chute", "socket": "hatch" if (k % 2 == 0 or not big) else "hatch.belly", "cue": ""} for k in range(crew)]}


# ---- the parachute ------------------------------------------------------------------------------------

def parachute(fine=True):
    """An airman under his parachute. The root is his harness (where the lines meet); the canopy opens above it."""
    m = Model("Parachute", "Parachute and airman (our own model)", "parachute",
              {"silk": "E8E5D8", "silk2": "D5D1C2", "line": "CFCBBB", "suit": "5B5A48", "skin": "C59B7C", "boots": "2B2622", "harness": "8D8462", "helmet": "4A3A2A"}, fine)
    m.part("chute", None, (0, 0, 0), kind="root", note="the harness point, where the lines meet; the game lets it down")
    m.part("canopy", "chute", (0, 0, 0), kind="free", note="canopy and lines; scaled from a streamer to full as it opens")
    m.part("airman", "chute", (0, 0, 0), axis=(0, 1, 0), kind="turn", limits=(-25, 25), note="he swings under it")
    R, top, skirt = 3.4, 7.6, 5.4                      # the canopy's radius, the height of its crown and of its skirt
    gores = 20 if fine else 10
    m.use("canopy")
    rows = 7 if fine else 4
    rings = []
    for j in range(rows + 1):
        a = (math.pi / 2.0) * (1.0 - j / rows) * 0.96 + 0.04      # from near the crown (a vent) down to the skirt
        ring = []
        for k in range(gores * 2):
            ang = 2 * math.pi * k / (gores * 2)
            puff = 1.0 + (0.045 if k % 2 else -0.02) * math.sin(math.pi * j / rows) + (0.03 if k % 2 else -0.03) * (j / rows) ** 3
            r = R * math.cos(a) * puff
            ring.append(Vector((r * math.cos(ang), r * math.sin(ang), skirt + (top - skirt) * math.sin(a))))
        rings.append(ring)
    n = gores * 2
    verts = [p for ring in rings for p in ring]
    faces = [(r * n + k, r * n + (k + 1) % n, (r + 1) * n + (k + 1) % n, (r + 1) * n + k) for r in range(rows) for k in range(n)]
    mid = Vector((0, 0, skirt + 0.5))
    m.add(verts, faces, lambda c, nn: "silk" if int((math.atan2(c.y, c.x) % (2 * math.pi)) / (2 * math.pi / gores)) % 2 == 0 else "silk2", True, lambda c: c - mid)
    for k in range(gores):
        ang = 2 * math.pi * (k + 0.0) / gores
        foot = rings[-1][(2 * k) % n]
        if fine or k % 2 == 0:
            m.rod(foot, Vector((0.16 * math.cos(ang), 0.16 * math.sin(ang), 0.25)), 0.012, "line", sides=3, caps=(False, False))
    # the airman: hanging in his harness, arms up to the risers
    m.use("airman")
    m.ball((0, 0, -0.42), (0.17, 0.2, 0.33), "suit", 10, 6)                    # body
    m.ball((0, 0, -0.02), (0.105, 0.1, 0.12), "helmet", 10, 6)                 # head
    m.ball((0.07, 0, -0.03), (0.06, 0.075, 0.085), "skin", 8, 5)               # face
    for sgn in (1.0, -1.0):
        m.rod((0, 0.2 * sgn, -0.2), (0.04, 0.2 * sgn, 0.24), 0.05, "suit", 0.04, sides=6)       # arm up
        m.rod((0.04, 0.2 * sgn, 0.24), (0.02, 0.17 * sgn, 0.27), 0.04, "skin", sides=5)
        m.rod((0.0, 0.1 * sgn, -0.7), (0.12, 0.11 * sgn, -1.12), 0.075, "suit", 0.06, sides=6)   # thigh
        m.rod((0.12, 0.11 * sgn, -1.12), (0.02, 0.11 * sgn, -1.52), 0.06, "suit", 0.05, sides=6)  # shin
        m.rod((0.02, 0.11 * sgn, -1.52), (0.16, 0.11 * sgn, -1.58), 0.05, "boots", sides=5)       # boot
        m.rod((0.0, 0.17 * sgn, -0.25), (0.0, 0.16 * sgn, 0.25), 0.014, "harness", sides=3)       # riser
    m.box((-0.17, 0, -0.38), (0.12, 0.3, 0.4), "harness")                       # the pack on his back
    m.socket("feet", (0.05, 0, -1.6), -Z, part="airman", note="where he touches down")
    m.socket("crown", (0, 0, top), Z, part="canopy")
    C = m.clips
    C["open"] = {"long": 1.8, "loop": False, "after": "hold", "about": "The parachute streams, fills and steadies. Start it as he leaves the machine.",
                 "tracks": [{"part": "canopy", "scale": [[0.0, 0.05, 0.05, 0.55], [0.5, 0.1, 0.1, 1.25], [0.9, 0.55, 0.55, 1.12], [1.15, 1.1, 1.1, 0.93], [1.45, 0.96, 0.96, 1.03], [1.8, 1.0, 1.0, 1.0]]},
                            {"part": "airman", "turn": [[0.0, 22.0], [0.9, 16.0], [1.25, -12.0], [1.8, 5.0]]}],
                 "events": [{"t": 1.0, "fx": "chute_crack", "socket": "crown", "cue": "parachute_open"}]}
    C["drift"] = {"long": 5.0, "loop": True, "about": "Coming down: he swings gently and the canopy breathes. The game lets him down at about 6 m a second, drifting with the wind.",
                  "tracks": [{"part": "airman", "turn": [[0.0, 5.0], [1.25, 0.0], [2.5, -5.0], [3.75, 0.0], [5.0, 5.0]]},
                             {"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [1.25, 1.02, 1.02, 0.985], [2.5, 1.0, 1.0, 1.0], [3.75, 0.985, 0.985, 1.012], [5.0, 1.0, 1.0, 1.0]]},
                             {"part": "chute", "rot": rot_keys(lambda t: q_of(roll=3.0 * math.sin(2 * math.pi * t / 5.0), pitch=2.0 * math.sin(2 * math.pi * t / 5.0 + 1.3)), 5.0, 0.25)}],
                  "events": []}
    C["collapse"] = {"long": 2.5, "loop": False, "after": "hold", "about": "Down: the canopy spills and settles on the sea or the ground.",
                     "tracks": [{"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [0.8, 1.15, 1.2, 0.5], [2.5, 1.3, 1.35, 0.06]], "move": [[0.0, 0, 0, 0], [2.5, 2.5, 0.6, -1.2]]}],
                     "events": [{"t": 0.0, "fx": "splash_small", "socket": "feet", "cue": ""}]}
    return m


# ---- the bomb -----------------------------------------------------------------------------------------

def bomb(fine=True):
    """A general-purpose bomb as it falls: a 250 kg case with its tail fins in a ring."""
    m = Model("Bomb", "Bomb (our own model)", "bomb", {"case": "3A3D38", "band": "A0803A", "fin": "2E302C"}, fine)
    m.part("bomb", None, (0, 0, 0), kind="root", note="the game drops it; the clip tips its nose down")
    sides = 14 if fine else 8
    prof = [(0.82, 0.0), (0.74, 0.07), (0.6, 0.14), (0.4, 0.175), (-0.1, 0.18), (-0.45, 0.13), (-0.8, 0.05)]
    rings = [[Vector((x, r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x, r in prof]
    m.skin(rings, lambda c, n: "band" if 0.42 < c.x < 0.56 else "case", caps=(True, True))
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        m.add([Vector((-0.42, 0, 0)) + d * 0.1, Vector((-0.86, 0, 0)) + d * 0.04, Vector((-0.86, 0, 0)) + d * 0.2, Vector((-0.6, 0, 0)) + d * 0.2], [(0, 1, 2, 3)], "fin", False)
    ring = [[Vector((x, 0.2 * math.cos(2 * math.pi * k / sides), 0.2 * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x in (-0.7, -0.86)]
    m.skin(ring, "fin")
    m.socket("nose", (0.82, 0, 0), X, note="it bursts here")
    m.clips["fall"] = {"long": 6.0, "loop": False, "after": "hold", "about": "Released level, its nose drops as it falls (the game carries it forward and down).",
                       "tracks": [{"part": "bomb", "rot": rot_keys(lambda t: q_of(pitch=-78.0 * smooth(t / 5.0), roll=40.0 * t), 6.0, 0.25)}], "events": []}
    return m


BUILDERS = {n: (lambda fine=True, n=n: convert(n, fine)) for n in ("Bf109", "Spitfire", "Zero", "Mustang", "Yak", "Fw190", "Me262", "Hurricane", "Warhawk", "Fighter",
                                                                  "Stuka", "Sturmovik", "He111", "Lancaster", "B17", "Bomber")}
