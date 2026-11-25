#!/usr/bin/env python3
"""Surf on the campaign map's coasts (release 2, tc.Map.Surf), its cloud test. No engine and nothing but Python's
own library.

The breakers are a material graph, written in Tools/build_content.py (build_sea) as calls that make nodes in
Unreal's editor on the Mac. A wrong pin there is seen by nobody until the Mac draws it. So this test runs the REAL
build_sea out of that file against a stand-in for the editor that only writes down each node and each wire, and then
works the graph out as numbers, point by point:

  1. M_Sea, built with no breakers, is node for node, wire for wire and place for place the M_Sea of before this
     change (its fingerprint is kept below).
  2. M_SeaSurf with its measure at nothing draws exactly M_Sea, everywhere.
  3. What M_SeaSurf draws is, to the last decimals, the sum the breakers were judged on in the cloud (the art
     director's pictures at screen size: written out again here, on its own, as plain arithmetic).
  4. The breakers behave: none over the open sea, none where the game puts the surf out (the coarse world sheet, the
     top of the zoom), a fifth of them in a bay's lee, never more foam than foam, parts of every crest missing, and
     far fewer of them than there is coast.
  5. They cost what the note says: three more reads of the Waves picture, and the count of nodes is printed.

Usage: Tools/sea_surf/run.sh   (or: python3 sea_surf_test.py [path to build_content.py])
"""
import ast
import hashlib
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CONTENT = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else os.path.join(HERE, "..", "build_content.py")

# M_Sea as build_sea made it before the breakers were written (the release 2 map line at c30701f2): the count of its
# nodes and the fingerprint of all of them (kind, place, settings, wires, in the order they are made).
SEA_BEFORE_NODES = 307
SEA_BEFORE = "129d9e077f61e5c94d5f447b3b2b0f56391931c93e5d20641ef12f37c10b9323"

# The measures the picture was judged on. A change to SEA_BREAKERS in build_content.py is a change to the picture:
# judge it again at screen size, on the sea's real colour, and change both.
JUDGED = {"Every": 5.0, "Uneven": 0.7, "From": 0.85, "To": 0.09, "Slows": 0.75, "Wob": 0.18, "BowLong": 520.0, "GapLong": 1100.0,
          "Gap": 0.43, "Lace": 0.55, "Front": 0.035, "Tail": 0.2, "Young": 0.35, "Gain": 2.0, "Lee": 0.2, "White": 0.9, "Deep": 0.08}

# ---- A stand-in for Unreal's editor: it writes the graph down --------------------------------------------------------


class Token:
    """Any name of the engine's that is only ever handed back to it: a kind of blend, a property, a sampler."""

    def __init__(self, name):
        self._name = name

    def __getattr__(self, key):
        if key.startswith("_"):
            raise AttributeError(key)
        return Token(self._name + "." + key)

    def __call__(self, *args, **kwargs):
        return Token(self._name + "()")

    def __eq__(self, other):
        return isinstance(other, Token) and other._name == self._name

    def __hash__(self):
        return hash(self._name)

    def __repr__(self):
        return self._name


class Texture:
    def __init__(self, path, settings, srgb):
        self.path, self.settings, self.srgb = path, settings, srgb

    def get_editor_property(self, key):
        return {"compression_settings": self.settings, "srgb": self.srgb}[key]

    def __repr__(self):
        return "Texture(" + self.path + ")"


ENGINE_TEXTURES = {
    "/Engine/EngineMaterials/T_Default_Material_Grid_M": Texture("Grid_M", Token("TextureCompressionSettings.TC_MASKS"), False),
    "/Engine/EngineResources/DefaultTexture": Texture("DefaultTexture", Token("TextureCompressionSettings.TC_DEFAULT"), True),
}


class Unreal:
    def __getattr__(self, key):
        if key.startswith("_"):
            raise AttributeError(key)
        if key.startswith("MaterialExpression"):
            return key[len("MaterialExpression"):]
        if key == "LinearColor":
            return lambda r=0.0, g=0.0, b=0.0, a=0.0: (float(r), float(g), float(b), float(a))
        if key == "load_asset":
            return ENGINE_TEXTURES.get
        if key in ("log", "log_error", "log_warning"):
            return lambda *args: None
        return Token(key)


class Node:
    def __init__(self, kind, index, x, y):
        self.kind, self.index, self.x, self.y = kind, index, x, y
        self.props, self.inputs = {}, {}

    def set_editor_property(self, key, value):
        self.props[key] = value

    def get_name(self):
        return "%s_%d" % (self.kind, self.index)


class Material:
    def __init__(self, name):
        self.name, self.nodes, self.outs, self.settings = name, [], {}, {}

    def set_editor_property(self, key, value):
        self.settings[key] = value

    def get_path_name(self):
        return "/Game/Campaign/Materials/" + self.name


class Editing:
    @staticmethod
    def create_material_expression(material, kind, x=0, y=0):
        n = Node(kind, len(material.nodes), x, y)
        material.nodes.append(n)
        return n

    @staticmethod
    def connect_material_expressions(source, output, target, target_input):
        target.inputs[target_input] = (source, output)
        return True

    @staticmethod
    def connect_material_property(source, output, prop):
        Editing.last.outs[repr(prop)] = (source, output)
        return True

    @staticmethod
    def layout_material_expressions(material):
        pass

    @staticmethod
    def recompile_material(material):
        pass


class Assets:
    made = []

    @staticmethod
    def create_asset(name, path, kind, factory):
        m = Material(name)
        Assets.made.append(m)
        Editing.last = m
        return m

    @staticmethod
    def does_asset_exist(path):
        return False

    @staticmethod
    def delete_asset(path):
        pass

    @staticmethod
    def save_asset(path, only_if_is_dirty=False):
        pass


def functions_of(path):
    """Every function the file defines at its top, and every plain number, word or table it names there: none of
    its work is run (it would make a level and every material)."""
    with open(path, encoding="utf-8") as f:
        tree = ast.parse(f.read(), path)
    keep = []
    for part in tree.body:
        if isinstance(part, ast.FunctionDef):
            keep.append(part)
        elif isinstance(part, ast.Assign):
            try:
                ast.literal_eval(part.value)
                keep.append(part)
            except (ValueError, SyntaxError):
                pass
    names = {"unreal": Unreal(), "mel": Editing, "eal": Assets, "asset_tools": Assets}
    exec(compile(ast.Module(body=keep, type_ignores=[]), path, "exec"), names)
    return names


def built(names, *args):
    names["build_sea"](*args)
    return Assets.made[-1]


def fingerprint(material):
    rows = [sorted((k, repr(v)) for k, v in material.settings.items())]
    for n in material.nodes:
        rows.append((n.kind, n.x, n.y, sorted((k, repr(v)) for k, v in n.props.items()),
                     sorted((pin, source.index, output) for pin, (source, output) in n.inputs.items())))
    rows.append(sorted((prop, source.index, output) for prop, (source, output) in material.outs.items()))
    return hashlib.sha256(repr(rows).encode("utf-8")).hexdigest()


# ---- The graph worked out as numbers ---------------------------------------------------------------------------------


def parts(v):
    return v if isinstance(v, tuple) else (v,)


def each(f, *values):
    """A sum done part by part, a plain number standing for as many parts as the other side has."""
    wide = max(len(parts(v)) for v in values)
    if wide == 1:
        return f(*[parts(v)[0] for v in values])
    spread = [parts(v) * wide if len(parts(v)) == 1 else parts(v) for v in values]
    if any(len(s) != wide for s in spread):
        raise ValueError("a sum of values of different widths")
    return tuple(f(*column) for column in zip(*spread))


def sat(v):
    return min(max(v, 0.0), 1.0)


class Point:
    """One place on the sea at one moment: what the engine would hand the material there."""

    def __init__(self, x, y, t, offshore, exposed, scalars=None):
        self.pos, self.t, self.offshore, self.exposed = (x, y, 0.0), t, offshore, exposed
        self.scalars = scalars or {}
        self.memo = {}

    def picture(self, name, u, v):
        if name == "Waves":
            return waves_picture(u, v)
        if name == "Water":
            return (0.02, 0.2, 0.3, self.offshore)
        if name == "Shore":
            return (self.exposed, 0.0, 0.0, 0.0)
        raise KeyError("a picture the test does not paint: " + name)


def work(node, point):
    if node.index in point.memo:
        return point.memo[node.index]
    p, kind = node.props, node.kind

    def fed(pin, const=None):
        if pin in node.inputs:
            return value(node.inputs[pin], point)
        if const is None or const not in p:
            raise KeyError("%s has nothing on its pin %r" % (node.get_name(), pin))
        return p[const]

    if kind == "TextureCoordinate":
        out = (0.5, 0.5)
    elif kind == "TextureSampleParameter2D":
        uv = fed("UVs") if "UVs" in node.inputs else (0.5, 0.5)
        out = point.picture(str(p["parameter_name"]), uv[0], uv[1])
    elif kind == "VectorParameter":
        out = p["default_value"]
    elif kind == "ScalarParameter":
        out = point.scalars.get(str(p["parameter_name"]), p["default_value"])
    elif kind == "Constant":
        out = p["r"]
    elif kind == "Constant2Vector":
        out = (p["r"], p["g"])
    elif kind == "Constant3Vector":
        out = p["constant"][:3]
    elif kind == "WorldPosition":
        out = point.pos
    elif kind == "Time":
        out = point.t
    elif kind == "ComponentMask":
        v = parts(fed(""))
        chosen = tuple(v[i] for i, key in enumerate("rgba") if p.get(key))
        out = chosen if len(chosen) > 1 else chosen[0]
    elif kind == "DotProduct":
        out = sum(a * b for a, b in zip(parts(fed("A")), parts(fed("B"))))
    elif kind == "AppendVector":
        out = parts(fed("A")) + parts(fed("B"))
    elif kind == "Panner":
        c = fed("Coordinate")
        out = (c[0] + point.t * p.get("speed_x", 0.0), c[1] + point.t * p.get("speed_y", 0.0))
    elif kind == "LinearInterpolate":
        out = each(lambda a, b, t: a + (b - a) * t, fed("A", "const_a"), fed("B", "const_b"), fed("Alpha", "const_alpha"))
    elif kind == "Multiply":
        out = each(lambda a, b: a * b, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Add":
        out = each(lambda a, b: a + b, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Subtract":
        out = each(lambda a, b: a - b, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Divide":
        out = each(lambda a, b: a / b, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Max":
        out = each(max, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Min":
        out = each(min, fed("A", "const_a"), fed("B", "const_b"))
    elif kind == "Saturate":
        out = each(sat, fed(""))
    elif kind == "OneMinus":
        out = each(lambda a: 1.0 - a, fed(""))
    elif kind == "Abs":
        out = each(abs, fed(""))
    elif kind == "Frac":
        out = each(lambda a: a - math.floor(a), fed(""))
    elif kind == "Sine":
        period = p.get("period", 1.0)
        out = each(lambda a: math.sin(2.0 * math.pi * a / period), fed(""))
    elif kind == "Power":
        exponent = fed("Exponent", "const_exponent")
        out = each(lambda a: max(a, 0.0) ** exponent, fed("Base"))
    else:
        raise KeyError("a kind of node the test cannot work out: " + kind)
    point.memo[node.index] = out
    return out


def value(wire, point):
    node, output = wire
    v = work(node, point)
    if node.kind in ("TextureSampleParameter2D", "VectorParameter"):
        if output in ("", "RGB"):
            return v[:3]
        return v["RGBA".index(output)]
    if output != "":
        raise KeyError("%s has no output %r" % (node.get_name(), output))
    return v


def surf_of(material):
    """The four places of the graph the test reads: the sea's colour with its foam on it (before the weather's sky
    and the paper map), how much foam, what colour the foam is, and the foam's sum before its strength."""
    surf_colour = [n for n in material.nodes if n.kind == "VectorParameter" and str(n.props["parameter_name"]) == "SurfColour"]
    assert len(surf_colour) == 1
    mixed = [n for n in material.nodes if n.kind == "LinearInterpolate" and n.inputs.get("A", (None,))[0] is surf_colour[0]]
    foam_colour = mixed[0] if mixed else surf_colour[0]
    last = [n for n in material.nodes if n.kind == "LinearInterpolate" and n.inputs.get("B", (None,))[0] is foam_colour]
    assert len(last) == 1 and len(mixed) <= 1
    foam = last[0].inputs["Alpha"]
    assert foam[0].kind == "Saturate" and foam[0].inputs[""][0].kind == "Multiply"
    return {"colour": (last[0], ""), "foam": foam, "foam_colour": (foam_colour, ""), "sum": foam[0].inputs[""][0].inputs["A"]}


def reached(material):
    seen, stack = set(), [source for source, _ in material.outs.values()]
    while stack:
        n = stack.pop()
        if n.index not in seen:
            seen.add(n.index)
            stack.extend(source for source, _ in n.inputs.values())
    return seen


# ---- The Waves picture, for the test: it repeats, as the game's does (CampaignTextures::MakeWaves) ---------------------

DICE = random.Random(1947)
SWELL = [(a, b, DICE.uniform(0.0, 2.0 * math.pi), DICE.uniform(0.7, 1.0)) for a, b in ((6, 0), (5, 2), (7, -2), (6, 3), (5, -3), (7, 1), (4, -1))]
CHOP = [(a, b, DICE.uniform(0.0, 2.0 * math.pi), DICE.uniform(0.7, 1.0)) for a, b in ((12, 0), (11, 4), (13, -4), (12, 6), (10, -5), (14, 2), (9, -2), (13, 5))]
SLOW = [(a, b, DICE.uniform(0.0, 2.0 * math.pi), h) for a, b, h in ((1, 2, 0.5), (3, -1, 0.3), (2, 3, 0.2), (5, 1, 0.12))]


def trains(field, u, v, usual_of=0.35):
    total = sum(h * math.sin(2.0 * math.pi * (a * u + b * v) + phase) for a, b, phase, h in field)
    usual = math.sqrt(sum(h * h for _, _, _, h in field) / 2.0)
    return min(max(total / usual * usual_of, -1.0), 1.0) * 0.5 + 0.5


def waves_picture(u, v):
    flecks = sat(math.sin(2.0 * math.pi * (9 * u + 4 * v)) * math.sin(2.0 * math.pi * (5 * u - 8 * v)))
    return (trains(SWELL, u, v), trains(CHOP, u, v), flecks, trains(SLOW, u, v, 0.3))


# ---- The sum the breakers were judged on, written out on its own -----------------------------------------------------


def mix(a, b, t):
    return a + (b - a) * t


def frac(v):
    return v - math.floor(v)


def judged_sum(x, y, t, offshore, exposed, b, surf=0.45, much=1.0):
    wind = math.degrees(math.atan2(0.9, 0.25))

    def along(angle, length, speed):
        a = math.radians(wind + angle)
        return ((x * math.cos(a) + y * math.sin(a)) / length - speed / length * t, (-x * math.sin(a) + y * math.cos(a)) / length)

    chop = waves_picture(*along(25.0, 192.0, 5.0))[1] + waves_picture(*along(7.0, 150.0, 3.5))[1] - 1.0
    uneven = waves_picture(x / 700.0, y / 700.0)[3]
    # (the wash's line and its one faint roller: the sea of before)
    phase = t / 3.0 + uneven * 2.5
    wash = math.sin(2.0 * math.pi * phase) * 0.5 + 0.5
    inner, outer = mix(0.02, 0.05, wash), mix(0.075, 0.15, wash)
    line = sat((outer - offshore) / (outer - inner))
    strength = mix(0.72, 1.28, wash) * surf * 1.35 * mix(0.6, 1.0, exposed)
    roll = frac(phase + 0.75)
    rolling = sat(1.0 - abs(offshore - mix(0.32, 0.08, roll)) / 0.035) * sat(chop * 2.0 + 0.3) * math.sin(math.pi * roll) * 0.2
    # (the breakers)
    bow = waves_picture(*along(70.0, b["BowLong"], 1.5))
    gap_one = waves_picture(*along(160.0, b["GapLong"], 3.0))[0]
    gap_two = waves_picture(*along(-35.0, b["GapLong"] * 0.83, 2.0))[0]
    bows = (bow[3] - 0.5, bow[0] - 0.5, gap_two - 0.5)
    gaps = (gap_one, gap_two, 0.5 * gap_two - 0.1 * gap_one + 0.3)
    beat = t / b["Every"] + uneven * b["Uneven"]
    lace = mix(1.0, sat(chop * 2.5 + 0.55), b["Lace"])
    foams, crests, wholes = 0.0, 0.0, []
    for k in range(3):
        run = frac(beat + k / 3.0 + 0.4)
        off = offshore + bows[k] * b["Wob"] * sat(offshore * 6.0) - mix(b["From"], b["To"], run ** b["Slows"])
        grown = mix(b["Young"], 1.0, sat(run * 2.0))
        body = sat(1.0 - max(off / (grown * b["Tail"]), -off / b["Front"]))
        eased = sat((gaps[k] - b["Gap"]) / 0.1)
        whole = eased * eased * (3.0 - 2.0 * eased)
        life = sat(run / 0.25) * sat((1.0 - run) / 0.15) * whole
        foams += body * body * life * lace
        crests += sat(1.0 - abs(off) / b["Front"]) * life
        wholes.append(whole)
    much_here = much * mix(b["Lee"], 1.0, exposed) * sat((1.0 - offshore) / b["Deep"])
    breaking = foams * b["Gain"] * much_here
    white = sat(crests * much_here) * b["White"]
    return {"foam": sat((line + rolling + breaking) * strength), "sum": line + rolling + breaking, "before": line + rolling,
            "foam_colour": tuple(mix(s, w, white) for s, w in zip((0.46, 0.74, 0.9), (0.8, 0.9, 0.95))), "white": white,
            "breaking": breaking, "strength": strength, "wholes": wholes}


# ---- The checks ----------------------------------------------------------------------------------------------------

CHECKS, FAULTS = 0, 0


def check(ok, what):
    global CHECKS, FAULTS
    CHECKS += 1
    if not ok:
        FAULTS += 1
        print("FAULT: " + what)


def near(a, b, by=1.0e-9):
    return all(abs(p - q) <= by for p, q in zip(parts(a), parts(b))) and len(parts(a)) == len(parts(b))


def main():
    names = functions_of(CONTENT)
    if "--fingerprint" in sys.argv:
        m = built(names)
        print(len(m.nodes), fingerprint(m))
        return 0
    measures = names.get("SEA_BREAKERS")
    check(isinstance(measures, dict), "build_content.py names the breakers' measures (SEA_BREAKERS)")
    if not isinstance(measures, dict):
        print("%d checks, %d faults" % (CHECKS, FAULTS))
        return 1
    check(measures == JUDGED, "the breakers' measures are the ones the picture was judged on (change both, and judge the picture again)")
    b = measures

    sea = built(names)
    surf = built(names, "M_SeaSurf", measures)
    check(sea.name == "M_Sea" and surf.name == "M_SeaSurf", "the sea is M_Sea; the sea with breakers is a second material, M_SeaSurf")
    check(len(sea.nodes) == SEA_BEFORE_NODES and fingerprint(sea) == SEA_BEFORE,
          "M_Sea is node for node, wire for wire, the M_Sea of before (%d nodes, %s)" % (len(sea.nodes), fingerprint(sea)[:16]))
    check(fingerprint(built(names, "M_Sea", None)) == SEA_BEFORE and fingerprint(built(names, "M_Sea", {})) == SEA_BEFORE,
          "and so it is when no breakers are asked by name")

    def reads(material, name):
        return sum(1 for n in material.nodes if n.kind == "TextureSampleParameter2D" and str(n.props["parameter_name"]) == name)

    check(reads(surf, "Waves") == reads(sea, "Waves") + 3, "the breakers cost three more reads of the Waves picture (%d, where the sea has %d)" % (reads(surf, "Waves"), reads(sea, "Waves")))
    check(all(reads(surf, name) == reads(sea, name) for name in ("Water", "Shore", "Ripples", "Sky", "Weather")), "and no more read of any other picture")
    loose_sea, loose_surf = len(sea.nodes) - len(reached(sea)), len(surf.nodes) - len(reached(surf))
    check(loose_surf == loose_sea, "no node of the breakers is left unwired (%d loose in M_SeaSurf, %d in M_Sea)" % (loose_surf, loose_sea))
    scalars = lambda m: sorted(str(n.props["parameter_name"]) for n in m.nodes if n.kind == "ScalarParameter")
    vectors = lambda m: sorted(str(n.props["parameter_name"]) for n in m.nodes if n.kind == "VectorParameter")
    check(scalars(surf) == sorted(scalars(sea) + ["Breakers"]) and vectors(surf) == sorted(vectors(sea) + ["BreakerColour"]),
          "M_SeaSurf takes every number and colour M_Sea takes by name, and two of its own: Breakers and BreakerColour")
    kinds = lambda m: set(n.kind for n in m.nodes)
    check(kinds(surf) - kinds(sea) <= {"Power", "Max"}, "the breakers use no kind of node the content script has not used before (new to the sea: %s)" % ", ".join(sorted(kinds(surf) - kinds(sea))))

    at_sea, at_surf = surf_of(sea), surf_of(surf)
    dice = random.Random(25111947)
    points = []
    for i in range(1500):
        offshore = dice.choice((0.0, 1.0, dice.uniform(0.0, 1.0), dice.uniform(0.0, 1.0), dice.uniform(0.0, 0.3), dice.uniform(0.05, 0.9)))
        points.append((dice.uniform(-40000.0, 40000.0), dice.uniform(-20000.0, 20000.0), dice.uniform(0.0, 900.0), offshore, dice.choice((0.0, 1.0, dice.random()))))

    # 2. Its measure at nothing: M_Sea, exactly.
    same_off, same_dark, same_deep, worked = True, True, True, True
    for x, y, t, offshore, exposed in points:
        was = value(at_sea["colour"], Point(x, y, t, offshore, exposed))
        same_off = same_off and value(at_surf["colour"], Point(x, y, t, offshore, exposed, {"Breakers": 0.0})) == was
        dark = {"Surf": 0.0}
        same_dark = same_dark and value(at_surf["colour"], Point(x, y, t, offshore, exposed, dark)) == value(at_sea["colour"], Point(x, y, t, offshore, exposed, dark))
        same_deep = same_deep and value(at_surf["colour"], Point(x, y, t, 1.0, exposed)) == value(at_sea["colour"], Point(x, y, t, 1.0, exposed))
        worked = worked and len(parts(was)) == 3 and all(0.0 <= c <= 1.5 for c in was)
    check(worked, "the sea's colour can be worked out of the graph at every point tried (three parts, none wild)")
    check(same_off, "with the breakers' measure at nothing, M_SeaSurf's colour is M_Sea's to the last bit at %d points" % len(points))
    check(same_dark, "where the game puts the surf out (the coarse world sheet; the top of the zoom), no breaker shows")
    check(same_deep, "over the open sea, sixty units and more from any shore, no breaker shows at any moment")

    # 3. What it draws is the sum that was judged.
    worst_sea, worst_foam, worst_sum, worst_colour = 0.0, 0.0, 0.0, 0.0
    in_range, never_less = True, True
    for much in (1.0, 0.5, 2.0):
        for x, y, t, offshore, exposed in points[:900]:
            want = judged_sum(x, y, t, offshore, exposed, b, much=much)
            point = Point(x, y, t, offshore, exposed, {"Breakers": much})
            foam, total, colour = value(at_surf["foam"], point), value(at_surf["sum"], point), value(at_surf["foam_colour"], point)
            worst_foam, worst_sum = max(worst_foam, abs(foam - want["foam"])), max(worst_sum, abs(total - want["sum"]))
            worst_colour = max(worst_colour, max(abs(p - q) for p, q in zip(colour, want["foam_colour"])))
            plain = Point(x, y, t, offshore, exposed)
            worst_sea = max(worst_sea, abs(value(at_sea["sum"], plain) - want["before"]), abs(value(at_sea["foam"], plain) - sat(want["before"] * want["strength"])))
            in_range = in_range and 0.0 <= foam <= 1.0 and 0.0 <= want["white"] <= b["White"] + 1.0e-12 and all(0.0 <= c <= 1.0 for c in colour)
            never_less = never_less and total >= value(at_sea["sum"], plain) - 1.0e-12
    check(worst_sea < 1.0e-9, "the test's own arithmetic for the wash's line agrees with M_Sea's graph (worst %.2e)" % worst_sea)
    check(worst_sum < 1.0e-9, "M_SeaSurf's foam, before its strength, is the judged sum at 900 points and three measures (worst %.2e)" % worst_sum)
    check(worst_foam < 1.0e-9, "and after it (worst %.2e)" % worst_foam)
    check(worst_colour < 1.0e-9, "and the foam's colour, from the surf's pale blue toward white on a crest (worst %.2e)" % worst_colour)
    check(in_range, "foam is never under nothing nor over one, and its colour never out of a colour's range")
    check(never_less, "the breakers only ever add foam to the sea's own")

    # 4. They behave.
    lee_ok, lee_seen = True, 0
    for x, y, t, offshore, _ in points[:600]:
        open_coast, bay = judged_sum(x, y, t, offshore, 1.0, b), judged_sum(x, y, t, offshore, 0.0, b)
        lee_ok = lee_ok and abs(bay["breaking"] - b["Lee"] * open_coast["breaking"]) < 1.0e-12
        got = value(at_surf["sum"], Point(x, y, t, offshore, 0.0)) - value(at_sea["sum"], Point(x, y, t, offshore, 0.0))
        lee_ok = lee_ok and abs(got - bay["breaking"]) < 1.0e-9
        lee_seen += 1 if open_coast["breaking"] > 0.05 else 0
    check(lee_ok and lee_seen > 20, "in a bay's lee a breaker is a fifth of what it is on the open coast (%d breakers tried)" % lee_seen)

    # (a stretch of open coast watched for a while: how much of the band has a breaker on it, and how much is white)
    band, white, strong, lee_band, beach, holes, tries = 0, 0, 0, 0, 0, 0, 0
    watch = random.Random(4)
    for i in range(6000):
        x, y, t, offshore = watch.uniform(-30000.0, 30000.0), watch.uniform(-15000.0, 15000.0), watch.uniform(0.0, 600.0), watch.uniform(b["To"], b["From"])
        seen = judged_sum(x, y, t, offshore, 1.0, b)
        tries += 1
        band += 1 if seen["breaking"] * seen["strength"] > 0.1 else 0
        strong += 1 if seen["breaking"] * seen["strength"] > 0.35 else 0
        white += 1 if seen["white"] > 0.5 else 0
        bay = judged_sum(x, y, t, offshore, 0.0, b)
        lee_band += 1 if bay["breaking"] * bay["strength"] > 0.1 else 0
        beach += 1 if judged_sum(x, y, t, 0.01, 1.0, b)["breaking"] > 0.02 else 0
        holes += sum(1 for whole in seen["wholes"] if whole < 0.5)
    share = lambda n: 100.0 * n / tries
    check(8.0 < share(band) < 40.0, "on an open coast breakers are seen, and most of the water between the beach and their rising is still water (%.1f%% of it has foam on it)" % share(band))
    check(1.5 < share(white) < 15.0, "a white crest is a thin thing (%.1f%% of that water)" % share(white))
    check(share(strong) > 3.0, "and they are not faint: %.1f%% of that water is strongly foamed" % share(strong))
    check(share(lee_band) < share(band) / 4.0, "in a bay's lee far less of it (%.1f%%)" % share(lee_band))
    check(share(beach) < 1.0, "at the water's edge itself the wash's own line is left alone (%.2f%%)" % share(beach))
    check(30.0 < 100.0 * holes / (3.0 * tries) < 70.0, "parts of every crest are missing along its length: %.0f%% of it" % (100.0 * holes / (3.0 * tries)))

    # (a crest comes in: watched at one place, each set rises out by the shoal's edge and ends by the beach)
    comes = True
    for k in range(3):
        last = None
        for step in range(1, 100):
            run = step / 100.0
            at = mix(b["From"], b["To"], run ** b["Slows"])
            comes = comes and b["To"] <= at <= b["From"] and (last is None or at < last)
            last = at
    check(comes and b["From"] + 0.5 * b["Wob"] < 1.0 - b["Deep"] * 0.5, "a crest only ever comes in, from its rising to the beach, and rises inside the shoal water the picture knows")
    check(b["From"] * 60.0 > 40.0 and b["Every"] >= 4.0 and (b["From"] - b["To"]) * 60.0 / b["Every"] < 14.0,
          "she rises %.0f units out and comes in at %.1f units a second: a thing seen to move, not to flicker" % (b["From"] * 60.0, (b["From"] - b["To"]) * 60.0 / b["Every"]))

    print("Open coast: %.1f%% of the water between the beach and the rising has foam on it, %.1f%% strongly, %.1f%% a white crest; in a bay's lee %.1f%%." % (share(band), share(strong), share(white), share(lee_band)))
    print("M_Sea: %d nodes, %d picture reads. M_SeaSurf: %d nodes, %d picture reads." % (
        len(sea.nodes), sum(1 for n in sea.nodes if n.kind == "TextureSampleParameter2D"),
        len(surf.nodes), sum(1 for n in surf.nodes if n.kind == "TextureSampleParameter2D")))
    print("%d checks, %d faults" % (CHECKS, FAULTS))
    return 1 if FAULTS else 0


if __name__ == "__main__":
    sys.exit(main())
