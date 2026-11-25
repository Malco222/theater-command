"""A stand-in for Unreal's own Python module ("unreal"), so that Tools/build_content.py runs in the cloud with no engine
(the release 2 ocean plan, step 0: measure and guard rails; nothing on screen changes).

graph_pin.py puts one of these in sys.modules["unreal"] and runs build_content.py through it. Nothing is drawn or
compiled: every asset the script makes is recorded, with its own settings, every node of a material with its kind and
settings, every wire between nodes and every wire to the material's outputs, and every parameter of a material
parameter collection, so a material's graph can be pinned by a hash (graph_pin.py) and later checked node by node
(step 7's material_check.py).

What it models of the engine, because the script depends on it:
- Enums and classes are made as they are asked for (unreal.BlendMode, unreal.MaterialExpressionSaturate, ...). An enum's
  member equals only itself, as the engine's do, so build_content's sampler_for() and stand_in() compare as they do in
  the engine: TC_MASKS is not TC_NORMALMAP, and neither is a string or a number.
- The engine's own pictures the script loads (ENGINE_TEXTURES) carry the compression and sRGB the script tests. The first
  path stand_in() tries, T_Default_Material_Grid_M, holds plain numbers (masks, linear), so a picture of numbers stands
  on it; a picture of colours falls through to the next path, T_Default_MacroVariation (default, sRGB).
- does_asset_exist answers from EXISTING (empty unless the caller fills it): with nothing there, everything is built.
- A wire into a node's input that already has one replaces it, as the material editor does; the replaced wire is kept
  in the trace.

Unknown lower-case names (a function the engine may not have) fail at once, rather than recording nothing.
"""

import sys

# ---- What the run made ---------------------------------------------------------------------------------------------

ASSETS = []          # every asset made, in order (Thing objects with _asset set)
BY_PATH = {}         # their paths -> the asset
LOGS = []            # unreal.log and its kin, in order: (level, text)
LOADS = []           # unreal.load_asset: (path, found)
DELETED = []         # EditorAssetLibrary.delete_asset paths
SAVED = []           # EditorAssetLibrary.save_asset paths
EXISTING = set()     # the paths does_asset_exist says are there (none: everything is built)
QUIET = True         # unreal.log prints nothing unless told


# ---- Enums ----------------------------------------------------------------------------------------------------------

class Member:
    """One member of an enum. It equals only itself (the engine's members compare by identity of value within their
    enum, and never equal a string or a number)."""

    __slots__ = ("enum", "name")

    def __init__(self, enum, name):
        self.enum = enum
        self.name = name

    def __eq__(self, other):
        return self is other

    def __ne__(self, other):
        return self is not other

    def __hash__(self):
        return hash((self.enum, self.name))

    def __repr__(self):
        return f"{self.enum}.{self.name}"


def _is_member_name(name):
    """An enum member's name, as the engine's Python spells them: upper case with an underscore (BLEND_OPAQUE,
    MP_EMISSIVE_COLOR, TC_MASKS, SAMPLERTYPE_COLOR, TMVM_MIP_BIAS)."""
    return name[:1].isalpha() and name.upper() == name and any(c.isalpha() for c in name)


class Kind(type):
    """A class of the engine's, made as it is asked for. As an enum its members are made the same way, once each."""

    def __getattr__(cls, name):
        if name.startswith("__") or not _is_member_name(name):
            raise AttributeError(f"unreal.{cls.__name__} has no {name} (the stand-in makes only enum members on a class)")
        member = Member(cls.__name__, name)
        type.__setattr__(cls, name, member)
        return member


# ---- Objects, structs and assets ------------------------------------------------------------------------------------

class Thing(metaclass=Kind):
    """An object of one of the engine's classes: a struct (LinearColor), a factory, a material, a material's node, a
    picture. Its settings are recorded as they are set, the last one of a name standing."""

    def __init__(self, *args, **props):
        self._kind = type(self).__name__
        self._args = list(args)
        self._props = {}
        self._asset = None       # (name, folder) for an asset made by create_asset
        self._path = None        # its object path, for an asset (made or the engine's own)
        self._material = None    # the material a node belongs to
        self._index = None       # a node's place in its material
        self._trace = None       # an asset's own trace of what was done to it
        for key, value in props.items():
            self._props[key] = value

    # (the engine's names)
    def set_editor_property(self, name, value, *more):
        if not isinstance(name, str):
            raise TypeError(f"set_editor_property wants a property's name, not {name!r}")
        self._props[name] = value
        owner = self if self._trace is not None else self._material
        if owner is not None and owner._trace is not None:
            owner._trace.append(["set", self._index if self is not owner else None, name, value])

    def set_editor_properties(self, props):
        for key, value in props.items():
            self.set_editor_property(key, value)

    def get_editor_property(self, name):
        if name not in self._props:
            raise ValueError(f"the stand-in {self._kind} has no {name!r} set (the engine's default is not modelled)")
        return self._props[name]

    def get_name(self):
        if self._asset is not None:
            return self._asset[0]
        if self._path is not None:
            return self._path.rsplit("/", 1)[-1].split(".")[0]
        if self._index is not None:
            return f"{self._kind}_{self._index}"
        return self._kind

    def get_path_name(self):
        if self._path is None:
            raise ValueError(f"the stand-in {self._kind} has no path")
        return self._path

    def get_class(self):
        return type(self)

    def __repr__(self):
        if self._path is not None:
            return f"<{self._kind} {self._path}>"
        return f"<{self._kind} {self._args!r} {self._props!r}>"


_CLASSES = {}


def _class(name):
    made = _CLASSES.get(name)
    if made is None:
        made = Kind(name, (Thing,), {})
        _CLASSES[name] = made
    return made


# ---- The engine's own pictures ----------------------------------------------------------------------------------------

ENGINE_TEXTURES = {
    # path: (compression, sRGB). The first stand_in() tries holds numbers; the rest hold colours.
    "/Engine/EngineMaterials/T_Default_Material_Grid_M": ("TC_MASKS", False),
    "/Engine/EngineMaterials/T_Default_MacroVariation": ("TC_DEFAULT", True),
    "/Engine/EngineResources/DefaultTexture": ("TC_DEFAULT", True),
    "/Engine/MapTemplates/TilingNoise05": ("TC_DEFAULT", True),
    "/Engine/EngineResources/WhiteSquareTexture": ("TC_DEFAULT", True),
    "/Engine/EngineMaterials/PaperNormal": ("TC_NORMALMAP", False),
}

_LOADED = {}


def load_asset(path, *more):
    if path in BY_PATH:
        LOADS.append((path, True))
        return BY_PATH[path]
    if path in ENGINE_TEXTURES:
        texture = _LOADED.get(path)
        if texture is None:
            settings, srgb = ENGINE_TEXTURES[path]
            texture = _class("Texture2D")()
            texture._path = path
            texture._props["compression_settings"] = getattr(_class("TextureCompressionSettings"), settings)
            texture._props["srgb"] = srgb
            _LOADED[path] = texture
        LOADS.append((path, True))
        return texture
    # (the engine answers None for what is not there; the stand-in does not know the engine's whole content, so this
    # is recorded and graph_pin refuses a run that asked for something it does not know)
    LOADS.append((path, False))
    return None


def log(text):
    LOGS.append(("log", str(text)))
    if not QUIET:
        print(text)


def log_warning(text):
    LOGS.append(("warning", str(text)))
    if not QUIET:
        print("warning:", text)


def log_error(text):
    LOGS.append(("error", str(text)))
    if not QUIET:
        print("error:", text)


# ---- Assets ------------------------------------------------------------------------------------------------------------

class _AssetTools:
    def create_asset(self, asset_name, package_path, asset_class, factory):
        if not isinstance(asset_class, Kind):
            raise TypeError(f"create_asset wants an unreal class, not {asset_class!r}")
        path = f"{package_path}/{asset_name}"
        if path in BY_PATH and path not in DELETED:
            raise RuntimeError(f"create_asset: {path} is already there (delete it first, as new_material does)")
        asset = asset_class()
        asset._asset = (asset_name, package_path)
        asset._path = f"{path}.{asset_name}"
        asset._trace = [["create_asset", asset_name, package_path, asset_class.__name__, factory]]
        asset._nodes = []
        asset._wires = {}        # (node, input) -> (source node, output)
        asset._outputs = {}      # property member -> (source node, output)
        asset._done = []         # layout, recompile, save
        asset._factory = factory
        ASSETS.append(asset)
        BY_PATH[path] = asset
        BY_PATH[asset._path] = asset
        return asset


class AssetToolsHelpers(Thing):
    @staticmethod
    def get_asset_tools():
        return _TOOLS


_TOOLS = _AssetTools()


def _node_of(material, node, what):
    if not isinstance(node, Thing) or node._material is not material:
        raise RuntimeError(f"{what}: {node!r} is not a node of {material.get_name()}")
    return node


class MaterialEditingLibrary(Thing):
    @staticmethod
    def create_material_expression(material, expression_class, node_pos_x=0, node_pos_y=0):
        if material is None or material._trace is None:
            raise RuntimeError("create_material_expression wants a material made by create_asset")
        if not isinstance(expression_class, Kind) or not expression_class.__name__.startswith("MaterialExpression"):
            raise TypeError(f"create_material_expression wants a MaterialExpression class, not {expression_class!r}")
        node = expression_class()
        node._material = material
        node._index = len(material._nodes)
        node._pos = (node_pos_x, node_pos_y)
        material._nodes.append(node)
        material._trace.append(["node", node._index, expression_class.__name__, node_pos_x, node_pos_y])
        return node

    @staticmethod
    def connect_material_expressions(from_expression, from_output_name, to_expression, to_input_name):
        if from_expression is None or to_expression is None:
            raise RuntimeError("connect_material_expressions: a node is None")
        material = to_expression._material
        _node_of(material, from_expression, "connect_material_expressions")
        _node_of(material, to_expression, "connect_material_expressions")
        key = (to_expression._index, to_input_name)
        before = material._wires.get(key)
        material._wires[key] = (from_expression._index, from_output_name)
        material._trace.append(["wire", from_expression._index, from_output_name, to_expression._index, to_input_name,
                                None if before is None else list(before)])
        return True

    @staticmethod
    def connect_material_property(from_expression, from_output_name, property_):
        if from_expression is None:
            raise RuntimeError("connect_material_property: the node is None")
        material = from_expression._material
        _node_of(material, from_expression, "connect_material_property")
        if not isinstance(property_, Member) or property_.enum != "MaterialProperty":
            raise TypeError(f"connect_material_property wants a MaterialProperty, not {property_!r}")
        before = material._outputs.get(property_)
        material._outputs[property_] = (from_expression._index, from_output_name)
        material._trace.append(["output", from_expression._index, from_output_name, property_,
                                None if before is None else list(before)])
        return True

    @staticmethod
    def layout_material_expressions(material):
        material._done.append("layout")
        material._trace.append(["layout"])

    @staticmethod
    def recompile_material(material):
        material._done.append("recompile")
        material._trace.append(["recompile"])


class EditorAssetLibrary(Thing):
    @staticmethod
    def does_asset_exist(asset_path):
        return asset_path in EXISTING

    @staticmethod
    def delete_asset(asset_path):
        DELETED.append(asset_path)
        EXISTING.discard(asset_path)
        return True

    @staticmethod
    def save_asset(asset_path, only_if_is_dirty=True):
        SAVED.append(asset_path)
        asset = BY_PATH.get(asset_path)
        if asset is not None and asset._trace is not None:
            asset._done.append("save")
            asset._trace.append(["save", only_if_is_dirty])
        return True


# ---- The module's own names ---------------------------------------------------------------------------------------------

_OWN = {
    "AssetToolsHelpers": AssetToolsHelpers,
    "MaterialEditingLibrary": MaterialEditingLibrary,
    "EditorAssetLibrary": EditorAssetLibrary,
}
for _name, _cls in _OWN.items():
    _CLASSES[_name] = _cls


def __getattr__(name):
    """unreal.X: an engine class (made as asked); never a lower-case name the stand-in does not have."""
    if name.startswith("__"):
        raise AttributeError(name)
    if name[:1].isupper():
        return _class(name)
    raise AttributeError(f"the stand-in unreal module has no {name!r} (add it to Tools/sea_waves/fake_unreal.py if the engine has it)")


def is_member(value):
    return isinstance(value, Member)


def is_thing(value):
    return isinstance(value, Thing)


def materials():
    """The materials made, in the order they were made."""
    return [a for a in ASSETS if a._kind == "Material"]


if __name__ == "__main__":
    sys.exit("fake_unreal.py is a module: run Tools/sea_waves/graph_pin.py")
