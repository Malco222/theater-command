"""
Creates the few Unreal assets the campaign needs that cannot be written as text:
three small materials and one empty level.

Everything else on the campaign table is built in C++ from the campaign data
when the game starts, so these assets are tiny and rarely change. Run it with
Tools/build_content.sh after changing this file. It is safe to run again: it
replaces what it made before.
"""
import unreal

MATERIALS = "/Game/Campaign/Materials"
LEVEL = "/Game/Campaign/Maps/CampaignMap"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def new_material(name):
    path = f"{MATERIALS}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = asset_tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"could not create {path}")
    return material


def scalar(material, name, default, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def vector(material, name, default, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*default))
    return node


def finish(material):
    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    eal.save_asset(material.get_path_name(), only_if_is_dirty=False)
    unreal.log(f"Theater Command: wrote {material.get_path_name()}")


def connect(source, output, target, target_input):
    if not mel.connect_material_expressions(source, output, target, target_input):
        raise RuntimeError(f"could not connect {source.get_name()}.{output} to {target.get_name()}.{target_input}")


def to_property(source, output, prop):
    if not mel.connect_material_property(source, output, prop):
        raise RuntimeError(f"could not connect {source.get_name()}.{output} to {prop}")


def node(material, kind, x, y, **properties):
    n = mel.create_material_expression(material, kind, x, y)
    for key, value in properties.items():
        n.set_editor_property(key, value)
    return n


def world_uv(material, tiling_name, tiling_default, x, y):
    """Texture coordinates taken from where a point is on the table, so a texture lies evenly over everything."""
    position = node(material, unreal.MaterialExpressionWorldPosition, x, y)
    flat = node(material, unreal.MaterialExpressionComponentMask, x + 220, y, r=True, g=True, b=False, a=False)
    connect(position, "", flat, "")
    scaled = node(material, unreal.MaterialExpressionMultiply, x + 440, y)
    connect(flat, "", scaled, "A")
    connect(scalar(material, tiling_name, tiling_default, x + 220, y + 120), "", scaled, "B")
    return scaled


def build_map_surface():
    """Land, sea and inked lines: the colour comes from the mesh itself, painted point by point in C++.
    The material adds what paint on a model has and a computer colour does not: a little unevenness and a slight tooth."""
    m = new_material("M_MapSurface")
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    # The mesh stores its colours the way a picture file does (sRGB); the renderer wants them linear.
    to_linear = node(m, unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    connect(colour, "", to_linear, "Base")

    # Unevenness: a soft noise at two sizes, multiplied together so the repeat does not show.
    noise = unreal.load_asset("/Engine/MapTemplates/TilingNoise05")
    coarse = node(m, unreal.MaterialExpressionTextureSample, -1100, 300, texture=noise, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(world_uv(m, "GrainTilingCoarse", 0.0011, -1800, 300), "", coarse, "UVs")
    fine = node(m, unreal.MaterialExpressionTextureSample, -1100, 600, texture=noise, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(world_uv(m, "GrainTilingFine", 0.0083, -1800, 600), "", fine, "UVs")
    both = node(m, unreal.MaterialExpressionAdd, -850, 450)
    connect(coarse, "R", both, "A")
    connect(fine, "R", both, "B")
    # 'both' runs from 0 to 2 around 1. Pull it toward 1 by the grain strength.
    one = node(m, unreal.MaterialExpressionConstant, -850, 650, r=1.0)
    grain = node(m, unreal.MaterialExpressionLinearInterpolate, -650, 450)
    connect(one, "", grain, "A")
    connect(both, "", grain, "B")
    connect(scalar(m, "Grain", 0.14, -850, 750), "", grain, "Alpha")
    painted = node(m, unreal.MaterialExpressionMultiply, -450, 0)
    connect(to_linear, "", painted, "A")
    connect(grain, "", painted, "B")
    to_property(painted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Tooth: the engine's paper bump, laid flat and faded in by a strength.
    paper = unreal.load_asset("/Engine/EngineMaterials/PaperNormal")
    bump = node(m, unreal.MaterialExpressionTextureSample, -1100, 950, texture=paper, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    connect(world_uv(m, "ToothTiling", 0.021, -1800, 950), "", bump, "UVs")
    flat = node(m, unreal.MaterialExpressionConstant3Vector, -1100, 1200, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    tooth = node(m, unreal.MaterialExpressionLinearInterpolate, -650, 1000)
    connect(flat, "", tooth, "A")
    connect(bump, "RGB", tooth, "B")
    connect(scalar(m, "Tooth", 0.35, -850, 1250), "", tooth, "Alpha")
    to_property(tooth, "", unreal.MaterialProperty.MP_NORMAL)

    to_property(scalar(m, "Roughness", 0.9, -450, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.25, -450, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_solid():
    """Pieces, the table and its fittings: one plain colour, set from C++. Glow keeps a banner readable when it is in shadow."""
    m = new_material("M_Solid")
    m.set_editor_property("used_with_instanced_static_meshes", True)
    colour = vector(m, "Color", (0.5, 0.5, 0.5, 1.0), -650, 0)
    to_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionMultiply, -350, 450)
    connect(colour, "", glow, "A")
    connect(scalar(m, "Glow", 0.0, -650, 500), "", glow, "B")
    to_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_property(scalar(m, "Roughness", 0.5, -650, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Metallic", 0.0, -650, 350), "", unreal.MaterialProperty.MP_METALLIC)
    finish(m)


def build_highlight():
    """The glow over a territory under the pointer or picked: light that is not shaded, see-through by the mesh's own alpha."""
    m = new_material("M_Highlight")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    colour = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -700, 0)
    glow = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 0)
    mel.connect_material_expressions(colour, "", glow, "A")
    mel.connect_material_expressions(scalar(m, "Brightness", 1.6, -700, 200), "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(colour, "A", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def build_level():
    """An empty level. The game mode lays the table when play begins."""
    if eal.does_asset_exist(LEVEL):
        unreal.log(f"Theater Command: {LEVEL} already exists, left alone")
        return
    world = asset_tools.create_asset("CampaignMap", "/Game/Campaign/Maps", unreal.World, unreal.WorldFactory())
    if world is None:
        raise RuntimeError(f"could not create {LEVEL}")
    eal.save_asset(LEVEL, only_if_is_dirty=False)
    unreal.log(f"Theater Command: wrote {LEVEL}")


build_map_surface()
build_solid()
build_highlight()
build_level()
unreal.log("Theater Command: content built")
