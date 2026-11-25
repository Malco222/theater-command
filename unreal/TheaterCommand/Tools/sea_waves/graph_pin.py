"""The materials' graphs, pinned (the release 2 ocean plan, step 0: measure and guard rails).

Runs Tools/build_content.py in the cloud through the stand-in Unreal module (fake_unreal.py), with nothing there yet
(does_asset_exist False) and TC_REBUILD=all, so every material is built, and pins each material's graph by a SHA-256:
M_StageSea (Pearl Harbor's and Midway's sea, and the fleet actions' sea today), M_UnderSea, M_Wake, M_Highlight and the
other eleven the build makes. A later step that refactors build_content.py, or adds the fleet's own materials after
M_UnderSea, must leave every one of these hashes as pinned, byte for byte (Tools/sea_waves/graphs.sha).

The hash is of a canonical graph, not of the order the script happened to lay its nodes in:
- each node is named by a hash of its kind, its final settings and, for each input, the name of the node wired into it
  and which output (so a node's name holds its whole graph upstream);
- the material's own settings, its outputs (MP_...) and the multiset of its nodes, by name, make the material's graph;
- where the nodes are put on the canvas is left out, as build_content's finish() has the editor lay them out afresh
  (layout_material_expressions); a material never laid out would keep them, and they would be hashed.
So a pure refactor (helpers moved, nodes laid in another order) keeps every hash; any change to what a material
computes or how it is set (a constant, a wire, a picture, a blend mode) changes it.

Also checked on every run: the build loop's tuple keeps the 15 pinned names and builders, in order (release 2's own
materials may stand between them: the ground's after M_MapSurface, M_HitMark before M_UnderSea); the loop's materials
are built in its order, and any made outside it by blocks of their own are named; two runs give the same hashes; the hash does not move when the nodes are laid in
another order, and does move for a changed setting, a dropped wire, a re-pinned output or a changed material setting;
the stand-in pictures are read as the engine would read them (numbers as masks, colours as colour); no engine asset the
stand-in does not know was asked for; and the run logs no warning or error.

Usage (from the project, TheaterCommand): python3 -I Tools/sea_waves/graph_pin.py [--out DIR] [--write]
  --out DIR   where the canonical graphs (DIR/graphs/<name>.json) and the traces (<name>.trace.json) go
  --write     re-pins graphs.sha from this tree (only with the change it pins, in the same commit)
"""

import ast
import hashlib
import importlib.util
import json
import os
import runpy
import sys

sys.dont_write_bytecode = True  # (the stand-in module and the build script loaded by path: no __pycache__ left in the tree)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BUILD = os.path.join(ROOT, "Tools", "build_content.py")
PINS = os.path.join(HERE, "graphs.sha")

# The build loop's pinned entries as at 10f85ffb: (material, its builder), in order. Fleet entries may only follow
# M_UnderSea; release 2's other lanes put theirs between these, which leaves each of the 15 graphs as it is.
PINNED = [
    ("M_MapSurface", "build_map_surface"), ("M_Solid", "build_solid"), ("M_Highlight", "build_highlight"),
    ("M_Flag", "build_flag"), ("M_Land", "build_land"), ("M_Sea", "build_sea"), ("M_Wake", "build_wake"),
    ("M_FlakSmoke", "build_flak_smoke"), ("M_Beam", "build_beam"), ("M_FlakLit", "build_flak_lit"),
    ("M_Glow", "build_glow"), ("M_Cloud", "build_cloud"), ("M_DeckShade", "build_deck_shade"),
    ("M_StageSea", "build_stage_sea"), ("M_UnderSea", "build_under_sea"),
]
PINNED_NAMES = [name for name, _ in PINNED]

NUMBERS_PICTURE = "/Engine/EngineMaterials/T_Default_Material_Grid_M"
COLOUR_PICTURE = "/Engine/EngineMaterials/T_Default_MacroVariation"


# ---- Running the build ---------------------------------------------------------------------------------------------

def load_fake(tag):
    """A fresh stand-in module (its record empty), loaded by its path: python3 -I puts no folder on sys.path."""
    spec = importlib.util.spec_from_file_location(f"fake_unreal_{tag}", os.path.join(HERE, "fake_unreal.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_build(tag="run", rebuild="all", existing=()):
    """Runs build_content.py once through a fresh stand-in; returns the stand-in, holding what was made."""
    fake = load_fake(tag)
    fake.EXISTING.update(existing)
    before_unreal = sys.modules.get("unreal")
    before_rebuild = os.environ.get("TC_REBUILD")
    sys.modules["unreal"] = fake
    os.environ["TC_REBUILD"] = rebuild
    try:
        runpy.run_path(BUILD, run_name="__main__")
    finally:
        if before_unreal is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = before_unreal
        if before_rebuild is None:
            os.environ.pop("TC_REBUILD", None)
        else:
            os.environ["TC_REBUILD"] = before_rebuild
    return fake


# ---- The canonical graph ---------------------------------------------------------------------------------------------

def ser(fake, value):
    """A setting's value as plain JSON: an enum member, an asset by its path, a struct by its kind and values."""
    if fake.is_member(value):
        return {"enum": f"{value.enum}.{value.name}"}
    if isinstance(value, type) and issubclass(value, fake.Thing):
        return {"class": value.__name__}
    if fake.is_thing(value):
        if value._path is not None:
            return {"asset": value._path}
        return {"struct": value._kind, "args": [ser(fake, a) for a in value._args],
                "props": {k: ser(fake, v) for k, v in value._props.items()}}
    if isinstance(value, (list, tuple)):
        return [ser(fake, v) for v in value]
    if isinstance(value, dict):
        return {str(k): ser(fake, v) for k, v in value.items()}
    if isinstance(value, float) and value != value:
        return {"float": "nan"}
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    raise TypeError(f"a setting of a kind the pin does not know: {value!r}")


def text_of(data):
    return json.dumps(data, sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def digest(data):
    return hashlib.sha256(text_of(data).encode("ascii")).hexdigest()


def graph_of(fake, nodes, wires, outputs, props, kind, name, folder, factory, done):
    """The canonical graph of one material from its parts: nodes [(kind, props, pos)], wires {(node, input): (from, out)},
    outputs {MaterialProperty member: (from, out)}."""
    laid_out = "layout" in done
    inputs = [dict() for _ in nodes]
    for (to, pin), (src, out) in wires.items():
        inputs[to][pin] = (src, out)
    ids = [None] * len(nodes)
    bodies = [None] * len(nodes)
    state = [0] * len(nodes)  # 0 not reached, 1 on the way down, 2 named
    for root in range(len(nodes)):
        if state[root]:
            continue
        stack = [(root, False)]
        while stack:
            i, up = stack.pop()
            if up:
                node_kind, node_props, pos = nodes[i]
                body = {"kind": node_kind, "props": {k: ser(fake, v) for k, v in node_props.items()},
                        "inputs": {pin: {"from": ids[src], "out": out} for pin, (src, out) in inputs[i].items()}}
                if not laid_out:
                    body["pos"] = list(pos)
                bodies[i] = body
                ids[i] = digest(body)[:24]
                state[i] = 2
                continue
            if state[i] == 2:
                continue
            if state[i] == 1:
                raise ValueError(f"{name}: the graph has a loop through node {i}")
            state[i] = 1
            stack.append((i, True))
            for src, _ in inputs[i].values():
                if state[src] == 1:
                    raise ValueError(f"{name}: the graph has a loop through node {src}")
                if state[src] == 0:
                    stack.append((src, False))
    listed = sorted(({"id": ids[i], **bodies[i]} for i in range(len(nodes))), key=lambda n: (n["id"], text_of(n)))
    return {
        "asset": name, "folder": folder, "class": kind, "factory": ser(fake, factory),
        "properties": {k: ser(fake, v) for k, v in props.items()},
        "outputs": {f"{p.enum}.{p.name}": {"from": ids[src], "out": out} for p, (src, out) in outputs.items()},
        "nodes": listed,
        "done": list(done),
    }


def parts_of(asset):
    nodes = [(n._kind, dict(n._props), n._pos) for n in asset._nodes]
    return nodes, dict(asset._wires), dict(asset._outputs), dict(asset._props)


def canonical(fake, asset):
    name, folder = asset._asset
    if asset._kind == "Material":
        nodes, wires, outputs, props = parts_of(asset)
        return graph_of(fake, nodes, wires, outputs, props, asset._kind, name, folder, asset._factory, asset._done)
    # (another asset: a material parameter collection, a level; its settings, and an MPC's parameters by name)
    data = {"asset": name, "folder": folder, "class": asset._kind, "factory": ser(fake, asset._factory),
            "properties": {k: ser(fake, v) for k, v in asset._props.items()}, "done": list(asset._done)}
    if asset._kind == "MaterialParameterCollection":
        data["parameters"] = parameters_of(fake, asset)
    source = getattr(asset, "_source", None)
    if source is not None:  # (a picture brought in: the file, as a path in the project, and its SHA-256)
        data["imported"] = {"file": source[0], "sha256": source[1]}
    return data


def parameters_of(fake, collection):
    """Every parameter of a material parameter collection: (kind, name, default), from its scalar and vector lists."""
    out = []
    for key, kind in (("scalar_parameters", "scalar"), ("vector_parameters", "vector")):
        for p in collection._props.get(key, []) or []:
            props = p._props if fake.is_thing(p) else {}
            out.append({"kind": kind, "name": str(props.get("parameter_name")), "default": ser(fake, props.get("default_value"))})
    return out


def trace_of(fake, asset):
    return [ser(fake, step) for step in asset._trace]


# ---- The build loop --------------------------------------------------------------------------------------------------

def builder_of(node):
    """A build loop entry's builder as the checks name it: a builder named outright by its name; a lambda (release 2's
    graphics join builds M_LandManWorn and the crowd's material through one, ("M_LandManWorn", lambda:
    build_land_men_worn_or_tell(...))) as "lambda: " and the function it calls. A pinned material's builder must be
    named outright, so a lambda on one of the 15 never matches PINNED and fails the check that keeps them."""
    if isinstance(node, ast.Name):
        return node.id
    body = node.body
    if isinstance(body, ast.Call) and isinstance(body.func, ast.Name):
        return "lambda: " + body.func.id
    return "lambda"


def loop_entries():
    """The (name, builder) pairs of build_content.py's build loop, read from its source."""
    with open(BUILD, encoding="utf-8") as f:
        tree = ast.parse(f.read(), BUILD)
    loops = []
    for node in tree.body:
        if isinstance(node, ast.For) and isinstance(node.target, ast.Tuple) and \
                [getattr(e, "id", None) for e in node.target.elts] == ["name", "build"] and isinstance(node.iter, ast.Tuple):
            entries = []
            for elt in node.iter.elts:
                if not (isinstance(elt, ast.Tuple) and len(elt.elts) == 2 and isinstance(elt.elts[0], ast.Constant) and
                        isinstance(elt.elts[1], (ast.Name, ast.Lambda))):
                    raise SystemExit("graph_pin: the build loop holds an entry that is not (\"name\", builder)")
                entries.append((elt.elts[0].value, builder_of(elt.elts[1])))
            loops.append(entries)
    if len(loops) != 1:
        raise SystemExit(f"graph_pin: build_content.py should have one build loop over (name, build); found {len(loops)}")
    return loops[0]


# ---- Checks ---------------------------------------------------------------------------------------------------------------

class Checks:
    def __init__(self):
        self.count = 0
        self.failed = []

    def check(self, ok, what):
        self.count += 1
        if not ok:
            self.failed.append(what)
            print(f"  FAILED: {what}")
        return ok


def self_checks(fake, checks):
    """The hash itself: blind to the order nodes are laid in, alive to any change in what is computed."""
    stage = next(a for a in fake.materials() if a._asset[0] == "M_StageSea")
    name, folder = stage._asset
    nodes, wires, outputs, props = parts_of(stage)

    def hashed(n, w, o, p):
        return digest(graph_of(fake, n, w, o, p, "Material", name, folder, stage._factory, stage._done))

    base = hashed(nodes, wires, outputs, props)
    # (the nodes laid in another order: reversed, and every seventh first)
    count = len(nodes)
    for label, order in (("reversed", list(range(count - 1, -1, -1))),
                         ("every seventh first", [i for i in range(count) if i % 7 == 0] + [i for i in range(count) if i % 7])):
        new_of = {old: new for new, old in enumerate(order)}
        n2 = [nodes[old] for old in order]
        w2 = {(new_of[to], pin): (new_of[src], out) for (to, pin), (src, out) in wires.items()}
        o2 = {p: (new_of[src], out) for p, (src, out) in outputs.items()}
        checks.check(hashed(n2, w2, o2, props) == base, f"M_StageSea's hash does not move with its nodes laid in another order ({label})")
    # (a constant changed: the first node with a number among its settings)
    for i, (kind, nprops, pos) in enumerate(nodes):
        number = next((k for k, v in nprops.items() if isinstance(v, float)), None)
        if number is not None:
            n2 = list(nodes)
            changed = dict(nprops)
            changed[number] = nprops[number] + 1e-6
            n2[i] = (kind, changed, pos)
            checks.check(hashed(n2, wires, outputs, props) != base, f"M_StageSea's hash moves with a constant changed ({kind}.{number})")
            break
    else:
        checks.check(False, "M_StageSea has a node with a number among its settings, to change")
    first_wire = sorted(wires)[0]
    w2 = dict(wires)
    del w2[first_wire]
    checks.check(hashed(nodes, w2, outputs, props) != base, "M_StageSea's hash moves with a wire dropped")
    w3 = dict(wires)
    src, out = wires[first_wire]
    w3[first_wire] = (src, out + "x")
    checks.check(hashed(nodes, w3, outputs, props) != base, "M_StageSea's hash moves with a wire taken from another output")
    some_output = sorted(outputs, key=lambda p: p.name)[0]
    o2 = dict(outputs)
    src, out = outputs[some_output]
    o2[some_output] = ((src + 1) % count, out)
    checks.check(hashed(nodes, wires, o2, props) != base, "M_StageSea's hash moves with an output wired from another node")
    p2 = dict(props)
    p2["two_sided"] = not p2.get("two_sided", False)
    checks.check(hashed(nodes, wires, outputs, p2) != base, "M_StageSea's hash moves with a material setting changed")
    # (positions count only for a material never laid out)
    n2 = [(k, p, (x + 10, y)) for k, p, (x, y) in nodes]
    checks.check(hashed(n2, wires, outputs, props) == base, "M_StageSea's hash does not move with its nodes moved on the canvas (finish() lays them out)")


def picture_checks(fake, checks):
    """The stand-in pictures, as the engine would read them: numbers on the masks picture, colours on a colour one."""
    numbers = colours = 0
    wrong = []
    for material in fake.materials():
        for node in material._nodes:
            texture = node._props.get("texture")
            if not fake.is_thing(texture) or texture._path is None:
                continue
            sampler = node._props.get("sampler_type")
            if node._kind != "MaterialExpressionTextureSampleParameter2D":
                continue
            if texture._path == NUMBERS_PICTURE:
                numbers += 1
                if not (fake.is_member(sampler) and sampler.name == "SAMPLERTYPE_MASKS"):
                    wrong.append(f"{material._asset[0]} node {node._index} reads the numbers picture as {sampler!r}")
            elif texture._path == COLOUR_PICTURE:
                colours += 1
                if not (fake.is_member(sampler) and sampler.name == "SAMPLERTYPE_COLOR"):
                    wrong.append(f"{material._asset[0]} node {node._index} reads the colour picture as {sampler!r}")
    checks.check(numbers > 0, f"the pictures of numbers stand on {NUMBERS_PICTURE} ({numbers} reads)")
    checks.check(colours > 0, f"the pictures of colours stand on {COLOUR_PICTURE} ({colours} reads)")
    checks.check(not wrong, "every stand-in picture is read by the sampler its kind wants" + ("" if not wrong else ": " + "; ".join(wrong[:5])))
    # (an enum's member equals only itself, as the engine's)
    T = getattr(fake, "TextureCompressionSettings")
    checks.check(T.TC_MASKS == T.TC_MASKS and T.TC_MASKS != T.TC_NORMALMAP and T.TC_MASKS != "TC_MASKS" and T.TC_MASKS != 0,
                 "an enum member equals only itself")


def collection_check(checks):
    """A material parameter collection (step 7's MPC_FleetSea) as the build will make one: every parameter recorded by
    kind, name and default, and a node reading it names the collection by its path."""
    fake = load_fake("collection")
    tools = fake.AssetToolsHelpers.get_asset_tools()
    collection = tools.create_asset("MPC_Stand", "/Game/Stand", fake.MaterialParameterCollection, fake.MaterialParameterCollectionFactoryNew())
    one = fake.CollectionScalarParameter()
    one.set_editor_property("parameter_name", "Fold")
    one.set_editor_property("default_value", 1.0)
    two = fake.CollectionVectorParameter(parameter_name="Train0", default_value=fake.LinearColor(0.0, 0.0, 0.0, 0.5))
    collection.set_editor_property("scalar_parameters", [one])
    collection.set_editor_property("vector_parameters", [two])
    got = parameters_of(fake, collection)
    checks.check(got == [{"kind": "scalar", "name": "Fold", "default": 1.0},
                         {"kind": "vector", "name": "Train0", "default": {"struct": "LinearColor", "args": [0.0, 0.0, 0.0, 0.5], "props": {}}}],
                 "a collection's parameters are recorded by kind, name and default")
    m = tools.create_asset("M_Stand", "/Game/Stand", fake.Material, fake.MaterialFactoryNew())
    read = fake.MaterialEditingLibrary.create_material_expression(m, fake.MaterialExpressionCollectionParameter, 0, 0)
    read.set_editor_property("collection", collection)
    read.set_editor_property("parameter_name", "Train0")
    fake.MaterialEditingLibrary.connect_material_property(read, "", fake.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    data = canonical(fake, m)
    checks.check(data["nodes"][0]["props"]["collection"] == {"asset": "/Game/Stand/MPC_Stand.MPC_Stand"} and
                 "MaterialProperty.MP_WORLD_POSITION_OFFSET" in data["outputs"], "a node reading a collection names it by its path")
    try:
        fake.MaterialEditingLibrary.no_such_function
        checks.check(False, "the stand-in refuses a function it does not have")
    except AttributeError:
        checks.check(True, "the stand-in refuses a function it does not have")


def read_pins():
    pins = {}
    if not os.path.exists(PINS):
        return pins
    with open(PINS, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            sha, name = line.split()
            pins[name] = sha
    return pins


def write_pins(hashes):
    with open(BUILD, "rb") as f:
        script = hashlib.sha256(f.read()).hexdigest()
    lines = [
        "# The materials Tools/build_content.py builds, each pinned by the SHA-256 of its canonical graph (graph_pin.py).",
        "# The release 2 ocean plan, step 0: pinned at 10f85ffb, before any edit to build_content.py. A later step may add",
        "# materials after M_UnderSea, but leaves every hash here as it is; a change here lands only with the change it pins.",
        "# (The graphs as fake_unreal.py stands in for the engine, its pictures included: a change to that model re-pins",
        "# here, in the same commit.)",
        f"# (pinned from build_content.py {script[:16]}...)",
    ]
    lines += [f"{hashes[name]}  {name}" for name in PINNED_NAMES]
    with open(PINS, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main(argv):
    out = os.environ.get("SEA_WAVES_OUT", os.path.join(os.environ.get("TMPDIR", "/tmp"), "sea_waves"))
    write = False
    args = list(argv)
    while args:
        a = args.pop(0)
        if a == "--out":
            out = args.pop(0)
        elif a == "--write":
            write = True
        else:
            raise SystemExit(f"graph_pin: unknown argument {a}")
    folder = os.path.join(out, "graphs")
    os.makedirs(folder, exist_ok=True)
    checks = Checks()

    # The build loop's head, from the source.
    entries = loop_entries()
    kept = [e for e in entries if e[0] in PINNED_NAMES]
    checks.check(kept == PINNED,
                 "the build loop's tuple keeps the 15 pinned (name, builder) pairs, in order: " + ", ".join(f"{n} ({b})" for n, b in kept))
    checks.check(len({n for n, _ in entries}) == len(entries), "no material is named twice in the build loop")

    first = run_build("first")
    second = run_build("second")
    built = [a._asset[0] for a in first.materials()]
    looped = [n for n, _ in entries]
    outside = [n for n in built if n not in looped]
    checks.check([n for n in built if n in PINNED_NAMES] == PINNED_NAMES, "the run builds the 15 pinned materials, in the loop's order")
    checks.check([n for n in built if n in looped] == looped,
                 f"the run builds every material of the loop, in its order ({len(built) - len(outside)} of {len(looped)})")
    checks.check(len(set(built)) == len(built), f"the run makes each material once ({len(built)}; outside the loop, by blocks "
                 f"of their own: {', '.join(outside) if outside else 'none'})")
    unknown = sorted({p for p, found in first.LOADS if not found})
    checks.check(not unknown, "every engine asset the build loads is one the stand-in knows" + ("" if not unknown else ": " + ", ".join(unknown)))
    bad_logs = [t for level, t in first.LOGS if level != "log"]
    checks.check(not bad_logs, "the build logs no warning or error" + ("" if not bad_logs else ": " + "; ".join(bad_logs[:3])))
    checks.check(any("content built" in t for _, t in first.LOGS), "the build runs to its end (\"content built\")")
    checks.check(any(a._kind == "World" for a in first.ASSETS), "the build makes the campaign's level too (build_level)")
    for material in first.materials():
        done = material._done
        checks.check(done[:2] == ["layout", "recompile"] and "save" in done, f"{material._asset[0]} is laid out, recompiled and saved (finish)")

    hashes = {}
    for asset in first.ASSETS:
        name = asset._asset[0]
        data = canonical(first, asset)
        hashes[name] = digest(data)
        with open(os.path.join(folder, f"{name}.json"), "w", encoding="utf-8") as f:
            json.dump(data, f, indent=1, sort_keys=True)
            f.write("\n")
        with open(os.path.join(folder, f"{name}.trace.json"), "w", encoding="utf-8") as f:
            json.dump(trace_of(first, asset), f)
            f.write("\n")
    again = {a._asset[0]: digest(canonical(second, a)) for a in second.ASSETS}
    checks.check(again == hashes, "two runs of the build give the same graphs, every asset")
    self_checks(first, checks)
    picture_checks(first, checks)
    collection_check(checks)

    if write:
        write_pins(hashes)
        print(f"graph_pin: graphs.sha written for the {len(PINNED_NAMES)} pinned materials")
    pins = read_pins()
    checks.check(sorted(pins) == sorted(PINNED_NAMES), f"graphs.sha pins exactly the 15 materials ({len(pins)} pinned)")
    for name in PINNED_NAMES:
        nodes = len(next(a for a in first.ASSETS if a._asset[0] == name)._nodes)
        ok = checks.check(hashes.get(name) == pins.get(name), f"{name}'s graph as pinned ({nodes} nodes)")
        print(f"  {name:13s} {hashes.get(name, '?')[:16]}  {nodes:5d} nodes  {'as pinned' if ok else 'CHANGED (pinned ' + str(pins.get(name, 'nothing'))[:16] + ')'}")
    for name in sorted(set(hashes) - set(PINNED_NAMES)):
        print(f"  {name:13s} {hashes[name][:16]}  (not pinned)")
    print(f"graph_pin: {checks.count} checks, {len(checks.failed)} failed (graphs in {folder})")
    return 0 if not checks.failed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
