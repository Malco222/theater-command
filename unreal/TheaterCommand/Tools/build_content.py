"""
Creates the few Unreal assets the campaign needs that cannot be written as text:
three small materials and one empty level.

Everything else on the campaign table is built in C++ from the campaign data
when the game starts, so these assets are tiny and rarely change. Run it with
Tools/build_content.sh after changing this file. It makes what is missing and
leaves the rest alone; TC_REBUILD=all (or a material's name) remakes them.
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
    # A tint over the whole of it, white unless C++ says otherwise: the woods take a cool one by moonlight, so that
    # the yellow of autumn does not shine out of the dark like lamps.
    tinted = node(m, unreal.MaterialExpressionMultiply, -250, 0)
    connect(painted, "", tinted, "A")
    connect(vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -450, -200), "", tinted, "B")
    to_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

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
    # (a tint, white unless C++ says otherwise: the province lines go blue-grey by moonlight)
    tinted = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 0)
    mel.connect_material_expressions(glow, "", tinted, "A")
    mel.connect_material_expressions(vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -450, -160), "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # (the inked lines of the map use this too, with "Opacity" turned down as the eye rises)
    faded = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 200)
    mel.connect_material_expressions(colour, "A", faded, "A")
    mel.connect_material_expressions(scalar(m, "Opacity", 1.0, -700, 320), "", faded, "B")
    mel.connect_material_property(faded, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def build_flag():
    """A nation's flag on the cloth of a standard. The picture is painted in C++ and handed in as 'Flag'.
    The cloth shows from both sides and, like the old plain banners, carries a little light of its own so it reads in shadow."""
    m = new_material("M_Flag")
    m.set_editor_property("two_sided", True)
    picture = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0,
                   parameter_name="Flag",
                   texture=unreal.load_asset("/Engine/EngineResources/DefaultTexture"),
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    to_property(picture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionMultiply, -350, 300)
    connect(picture, "RGB", glow, "A")
    connect(scalar(m, "Glow", 0.5, -700, 350), "", glow, "B")
    to_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_property(scalar(m, "Roughness", 0.75, -700, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


# ---- Helpers for the larger materials: small sums written the way they are said ----------------

def _feed(material, target, pin, value):
    """Connects something to a pin of a node. A number is set as the pin's constant; a (node, output) pair or a node is wired."""
    if isinstance(value, (int, float)):
        target.set_editor_property("const_" + pin.lower(), float(value))
        return
    source, output = value if isinstance(value, tuple) else (value, "")
    connect(source, output, target, pin)


def lerp(material, a, b, alpha, x=0, y=0):
    n = node(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    _feed(material, n, "A", a)
    _feed(material, n, "B", b)
    _feed(material, n, "Alpha", alpha)
    return n


def times(material, a, b, x=0, y=0):
    n = node(material, unreal.MaterialExpressionMultiply, x, y)
    _feed(material, n, "A", a)
    _feed(material, n, "B", b)
    return n


def plus(material, a, b, x=0, y=0):
    n = node(material, unreal.MaterialExpressionAdd, x, y)
    _feed(material, n, "A", a)
    _feed(material, n, "B", b)
    return n


def pair(material, a, b, x=0, y=0):
    """Two numbers side by side, as one value with two parts."""
    n = node(material, unreal.MaterialExpressionAppendVector, x, y)
    connect(*(a if isinstance(a, tuple) else (a, "")), n, "A")
    connect(*(b if isinstance(b, tuple) else (b, "")), n, "B")
    return n


def one_in(material, kind, source, x=0, y=0):
    n = node(material, kind, x, y)
    connect(*(source if isinstance(source, tuple) else (source, "")), n, "")
    return n


def leaning(material, sample, first, second, x=0, y=0):
    """Two channels of a picture that hold which way a surface leans, from 0..1 back to -1..1."""
    both = pair(material, (sample, first), (sample, second), x, y)
    return plus(material, times(material, both, 2.0, x + 200, y), -1.0, x + 400, y)


def sampler_for(texture):
    """The way a material must be told to read a picture, which depends on how the picture is stored."""
    settings = texture.get_editor_property("compression_settings")
    srgb = texture.get_editor_property("srgb")
    T, S = unreal.TextureCompressionSettings, unreal.MaterialSamplerType
    if settings == T.TC_NORMALMAP:
        return S.SAMPLERTYPE_NORMAL
    if settings == T.TC_MASKS:
        return S.SAMPLERTYPE_MASKS
    if settings == T.TC_GRAYSCALE:
        return S.SAMPLERTYPE_GRAYSCALE if srgb else S.SAMPLERTYPE_LINEAR_GRAYSCALE
    if settings == T.TC_ALPHA:
        return S.SAMPLERTYPE_ALPHA
    return S.SAMPLERTYPE_COLOR if srgb else S.SAMPLERTYPE_LINEAR_COLOR


def stand_in(numbers):
    """A picture of the engine's to stand in for one the game paints when it starts: one that holds colours, or one that holds plain numbers."""
    wanted = (unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR) if numbers \
        else (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,)
    for path in ("/Engine/EngineMaterials/T_Default_Material_Grid_M", "/Engine/EngineMaterials/T_Default_MacroVariation",
                 "/Engine/EngineResources/DefaultTexture", "/Engine/MapTemplates/TilingNoise05", "/Engine/EngineResources/WhiteSquareTexture"):
        texture = unreal.load_asset(path)
        if texture is not None and sampler_for(texture) in wanted:
            return texture
    raise RuntimeError("no stand-in picture of the right kind was found among the engine's own")


def picture(material, name, uv, x, y, numbers=False):
    """A picture the game hands in by name when it starts."""
    texture = stand_in(numbers)
    n = node(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y,
             parameter_name=name, texture=texture, sampler_type=sampler_for(texture))
    if uv is not None:
        connect(uv, "", n, "UVs")
    return n


def build_land():
    """The land. Its colour comes from pictures the game paints when it starts (CampaignGround.cpp), one pixel
    for every point of the terrain: the bare ground, the nations' paint, the lie of the land and where rock,
    snow and sand show through. Over that goes fine detail that repeats: grass, tree-tops, rock, sand."""
    m = new_material("M_Land")
    m.set_editor_property("tangent_space_normal", False)  # the picture says which way the ground leans in the world
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -3000, 0)
    ground = picture(m, "Ground", uv, -2600, -500)
    tint = picture(m, "Tint", uv, -2600, -250)
    relief = picture(m, "Relief", uv, -2600, 0, numbers=True)
    masks = picture(m, "Masks", uv, -2600, 250, numbers=True)

    # The nation's colour is a stain over the bare ground: a wash everywhere (thicker for a nation of little colour,
    # which the Relief picture's alpha says) and a band along the national frontier (the Tint picture's alpha).
    # "Flat" lays the stain on whole, for the map view that shows the countries in flat colour.
    wash = times(m, scalar(m, "Wash", 0.34, -2300, -760), plus(m, (relief, "A"), 1.0, -2300, -700), -2150, -730)
    band = times(m, (tint, "A"), scalar(m, "Band", 0.4, -2300, -640), -2150, -640)
    stain = one_in(m, unreal.MaterialExpressionSaturate, plus(m, wash, band, -2050, -690), -1950, -690)
    stain = lerp(m, stain, 1.0, scalar(m, "Flat", 0.0, -2050, -600), -1850, -660)
    stain = times(m, stain, scalar(m, "Politics", 0.94, -1850, -570), -1750, -630)
    c = lerp(m, (ground, "RGB"), (tint, "RGB"), stain, -2000, -400)
    # The detail, at two sizes so that its repeat does not show. (It is read here, ahead of its use, because the
    # edges of the woods and the lie of the fields are roughened by it.)
    fine_uv = world_uv(m, "DetailTiling", 0.0052, -3000, 700)
    coarse_uv = world_uv(m, "DetailTilingCoarse", 0.00137, -3000, 1000)
    fine = picture(m, "Detail", fine_uv, -2300, 700, numbers=True)
    coarse = picture(m, "Detail", coarse_uv, -2300, 1000, numbers=True)
    cover = picture(m, "Cover", uv, -2600, 500, numbers=True)

    # Farmland: a quilt of fields on the low ground. Every field has a tone of its own, from pasture through
    # wheat to plough, read from a picture with one pixel to a field; the rows lean and wander a little so that it
    # is no chessboard. The quilt multiplies whatever is under it, so a nation's fields are still its own colour.
    # Small fields with hedges round them in the west, great open ones on the eastern plains.
    place = world_uv(m, "FieldTiling", 0.0444, -3000, 1500)  # (a small field is about two and a quarter map units long)
    east0 = node(m, unreal.MaterialExpressionComponentMask, -2900, 1500, r=True, g=False, b=False, a=False)
    connect(place, "", east0, "")
    south0 = node(m, unreal.MaterialExpressionComponentMask, -2900, 1600, r=False, g=True, b=False, a=False)
    connect(place, "", south0, "")
    # Every province's fields are turned by an angle of its own and are a little larger or smaller than the next
    # province's (a picture the game paints says how, read without smoothing): the province lines are the natural
    # breaks between one field system and another, and the land is no tiled floor.
    lie = picture(m, "Lie", uv, -3000, 1750, numbers=True)
    cosine = plus(m, times(m, (lie, "R"), 2.0, -2800, 1750), -1.0, -2700, 1750)
    sine = plus(m, times(m, (lie, "G"), 2.0, -2800, 1850), -1.0, -2700, 1850)
    size = plus(m, times(m, (lie, "B"), -0.7, -2800, 1950), 1.35, -2700, 1950)  # 1.21 (small blocks) down to 0.79 (large)
    east = times(m, plus(m, times(m, east0, cosine, -2600, 1500), times(m, south0, sine, -2600, 1560), -2500, 1500), size, -2400, 1500)
    south = times(m, plus(m, times(m, south0, cosine, -2600, 1640), times(m, times(m, east0, sine, -2600, 1700), -1.0, -2550, 1700), -2500, 1640), size, -2400, 1640)
    rows = pair(m, plus(m, east, times(m, south, 0.33, -2500, 1560), -2350, 1500),
                times(m, plus(m, south, times(m, east, -0.22, -2500, 1660), -2350, 1620), 1.389, -2200, 1620), -2050, 1550)
    rows = plus(m, rows, times(m, leaning(m, coarse, "R", "G", -2300, 1800), scalar(m, "FieldWander", 0.3, -2000, 1900), -1700, 1800), -1500, 1600)
    small = picture(m, "Fields", times(m, rows, 1.0 / 64.0, -1300, 1500), -1100, 1500, numbers=True)
    large = picture(m, "Fields", plus(m, times(m, rows, 0.42 / 64.0, -1300, 1700), 0.37, -1150, 1700), -950, 1700, numbers=True)
    tone = times(m, lerp(m, (small, "RGB"), (large, "RGB"), (cover, "G"), -750, 1600), 2.0, -600, 1600)
    quilt = lerp(m, 1.0, tone, times(m, (cover, "R"), scalar(m, "FieldsStrength", 0.45, -750, 1800), -600, 1800), -400, 1650)
    # Hedges: a thin dark line round each small field, seen only close to.
    part = one_in(m, unreal.MaterialExpressionFrac, rows, -1300, 2000)
    back = one_in(m, unreal.MaterialExpressionOneMinus, part, -1150, 2100)
    near_edge = node(m, unreal.MaterialExpressionMin, -1000, 2000)
    connect(part, "", near_edge, "A")
    connect(back, "", near_edge, "B")
    edge_x = node(m, unreal.MaterialExpressionComponentMask, -850, 2000, r=True, g=False, b=False, a=False)
    connect(near_edge, "", edge_x, "")
    edge_y = node(m, unreal.MaterialExpressionComponentMask, -850, 2100, r=False, g=True, b=False, a=False)
    connect(near_edge, "", edge_y, "")
    edge = node(m, unreal.MaterialExpressionMin, -700, 2050)
    connect(edge_x, "", edge, "A")
    connect(edge_y, "", edge, "B")
    hedge_line = one_in(m, unreal.MaterialExpressionOneMinus,
                        one_in(m, unreal.MaterialExpressionSaturate, times(m, edge, scalar(m, "HedgeFine", 16.0, -700, 2200), -550, 2050), -400, 2050), -250, 2050)
    hedged = one_in(m, unreal.MaterialExpressionOneMinus,
                    times(m, times(m, hedge_line, scalar(m, "HedgeStrength", 0.0, -250, 2200), -100, 2100),
                          times(m, (cover, "R"), one_in(m, unreal.MaterialExpressionOneMinus, (cover, "G"), -250, 2350), -100, 2300), 50, 2150), 200, 2150)
    c = times(m, times(m, c, quilt, -1750, -330), hedged, -1650, -330)

    # Woods are one dark cloth laid over the paint: a standing mass with a ragged edge, thick enough to read as
    # woods on any nation's colour, fir-dark in the north and on the high ground. The woods take the nation's
    # stain too, at about half its strength, so that a nation's wood is still seen to be its own.
    # (its edge is broken at two scales, a fine one and a broad one: the broad one cuts clearings into a big wood
    # and throws a copse or two out beyond its edge; then the edge is made firm, a shape and not a fade)
    ragged = plus(m, times(m, plus(m, (fine, "G"), -0.5, -2500, -1250), scalar(m, "WoodsRag", 0.5, -2500, -1150), -2350, -1250),
                  times(m, plus(m, (coarse, "G"), -0.5, -2500, -1400), scalar(m, "WoodsRagBroad", 1.1, -2500, -1320), -2350, -1400), -2250, -1320)
    wooded = one_in(m, unreal.MaterialExpressionSaturate,
                    plus(m, times(m, plus(m, plus(m, (ground, "A"), ragged, -2200, -1250),
                                          -0.5, -2050, -1250), scalar(m, "WoodsSharp", 6.0, -2050, -1150), -1900, -1250), 0.5, -1750, -1250), -1600, -1250)
    # (a warm stain, red or orange, turns dark green to brown-black: the woods of such a nation take less of it)
    tint_r = node(m, unreal.MaterialExpressionComponentMask, -2500, -1550, r=True, g=False, b=False, a=False)
    connect(tint, "RGB", tint_r, "")
    tint_g = node(m, unreal.MaterialExpressionComponentMask, -2500, -1480, r=False, g=True, b=False, a=False)
    connect(tint, "RGB", tint_g, "")
    warm = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, tint_r, times(m, tint_g, -1.0, -2350, -1480), -2250, -1520), 2.5, -2150, -1520), -2050, -1520)
    cool = plus(m, times(m, warm, scalar(m, "WoodsWarmLess", -0.45, -2050, -1440), -1950, -1500), 1.0, -1850, -1500)
    leaf = lerp(m, vector(m, "WoodsColour", (0.05, 0.09, 0.04, 1.0), -2000, -900),
                vector(m, "FirColour", (0.024, 0.056, 0.04, 1.0), -2000, -1050), (cover, "A"), -1800, -950)
    woods = lerp(m, leaf,
                 times(m, (tint, "RGB"), scalar(m, "WoodsStainDepth", 0.5, -2300, -1000), -2100, -1000),
                 times(m, times(m, stain, scalar(m, "WoodsStain", 0.5, -2300, -1080), -2100, -1080), cool, -1950, -1080), -1650, -950)
    c = lerp(m, c, woods,
             times(m, wooded, scalar(m, "WoodsStrength", 0.84, -2300, -900), -2000, -700), -1500, -500)
    # The rim of a wood: lighter on its sunward edge, and a strip of shade on the open ground in its lee.
    c = times(m, c, plus(m, 1.0, times(m, plus(m, (cover, "B"), -0.5, -1500, -700), scalar(m, "WoodsRim", 0.6, -1500, -600), -1350, -650), -1200, -650), -1100, -500)
    c = lerp(m, c, vector(m, "RockColour", (0.235, 0.205, 0.162, 1.0), -2000, -150), (masks, "R"), -1000, -400)
    c = lerp(m, c, vector(m, "SnowColour", (0.888, 0.871, 0.823, 1.0), -1700, -150), (masks, "G"), -900, -400)
    c = lerp(m, c, vector(m, "SandColour", (0.694, 0.597, 0.371, 1.0), -1400, -150), (masks, "B"), -800, -400)

    # Which kind of detail shows where: grass by default, tree-tops where it is wooded, rock where rock shows,
    # sand on the shore and in the desert, nothing on snow.
    sandy = one_in(m, unreal.MaterialExpressionSaturate, plus(m, (masks, "B"), (masks, "A"), -2300, 500), -2100, 500)

    def kind_of_ground(sample, x, y):
        v = lerp(m, (sample, "R"), (sample, "G"), wooded, x, y)
        v = lerp(m, v, (sample, "B"), (masks, "R"), x + 200, y)
        v = lerp(m, v, (sample, "A"), sandy, x + 400, y)
        return lerp(m, v, 0.5, (masks, "G"), x + 600, y)

    near = times(m, kind_of_ground(fine, -2000, 700), 2.0, -1200, 700)
    far = lerp(m, 1.0, times(m, kind_of_ground(coarse, -2000, 1000), 2.0, -1200, 1000), scalar(m, "DetailCoarse", 0.45, -1200, 1150), -1000, 1000)
    detail = lerp(m, 1.0, times(m, near, far, -800, 850), scalar(m, "DetailStrength", 0.8, -800, 1000), -600, 850)

    # Shaded as a printed relief map is, on top of whatever the lamp does.
    # (the picture's blue is how squarely the ground faces a low sun in the north-west, shadows and all: the sun's
    # share of the light is warm and the shade's cool, so a lit slope and a shaded one differ in colour too)
    sun = times(m, (relief, "B"), scalar(m, "SunReach", 2.2, -1300, 180), -1100, 100)
    light = plus(m, vector(m, "ShadeColour", (0.43, 0.48, 0.58, 1.0), -1100, 260),
                 times(m, vector(m, "SunColour", (0.63, 0.56, 0.45, 1.0), -1100, 420), sun, -900, 300), -700, 200)
    c = times(m, times(m, c, detail, -400, -400), light, -200, -400)
    to_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Which way the surface leans: the ground itself, and the tree-tops and the rock on it.
    bumps = picture(m, "Bumps", fine_uv, -2300, 1400, numbers=True)
    trees = times(m, times(m, leaning(m, bumps, "R", "G", -2000, 1400), wooded, -1400, 1400), scalar(m, "TreeBump", 0.55, -1600, 1550), -1200, 1400)
    crags = times(m, times(m, leaning(m, bumps, "B", "A", -2000, 1700), (masks, "R"), -1400, 1700), scalar(m, "RockBump", 0.5, -1600, 1850), -1200, 1700)
    lean = plus(m, plus(m, leaning(m, relief, "R", "G", -2000, 2000), trees, -1000, 1500), crags, -800, 1500)
    squared = node(m, unreal.MaterialExpressionDotProduct, -600, 1500)
    connect(lean, "", squared, "A")
    connect(lean, "", squared, "B")
    upward = one_in(m, unreal.MaterialExpressionSquareRoot,
                    one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, squared, -1.0, -400, 1500), 1.0, -250, 1500), -100, 1500), 50, 1500)
    normal = one_in(m, unreal.MaterialExpressionNormalize, pair(m, lean, upward, 200, 1500), 400, 1500)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    to_property(scalar(m, "Roughness", 0.92, -200, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.22, -200, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_sea():
    """The sea: its colour from a picture the game paints (pale over the shoals, dark over the deeps), glossy
    like poured resin, with two sets of small ripples sliding across each other so that the lamp glints and moves on it."""
    m = new_material("M_Sea")
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1800, 0)
    water = picture(m, "Water", uv, -1400, 0)
    # A thin white line of surf just off every shore: the picture's alpha is how far offshore a point is (1 at six
    # map units), and the surf lies between about a tenth of a unit and two thirds of one.
    rising = one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, (water, "A"), 40.0, -1150, -260), -0.48, -1000, -260), -850, -260)
    falling = one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, (water, "A"), -16.0, -1150, -140), 1.76, -1000, -140), -850, -140)
    # (how strong it is on each stretch of shore is in a second picture: it comes and goes, and is strongest where
    # the open ocean runs in)
    shore = picture(m, "Shore", uv, -1400, -500, numbers=True)
    surf = times(m, times(m, times(m, rising, falling, -700, -200), scalar(m, "Surf", 0.45, -850, -60), -550, -200), (shore, "R"), -400, -250)
    # The hour's grading of the water, all of it nothing by day: by moonlight the game greys it (lit by a blue moon,
    # blue water goes cobalt), tints it and lifts the deeps a little.
    weights = node(m, unreal.MaterialExpressionConstant3Vector, -1250, -760, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
    brightness = node(m, unreal.MaterialExpressionDotProduct, -1100, -760)
    connect(water, "RGB", brightness, "A")
    connect(weights, "", brightness, "B")
    greyed = lerp(m, (water, "RGB"), brightness, scalar(m, "Grey", 0.0, -1100, -880), -950, -760)
    graded = plus(m, times(m, greyed, vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -950, -960), -800, -760), vector(m, "Lift", (0.0, 0.0, 0.0, 1.0), -800, -960), -650, -760)
    colour = lerp(m, graded, vector(m, "SurfColour", (0.86, 0.9, 0.86, 1.0), -850, -400), surf, -400, -100)
    to_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    first_uv = node(m, unreal.MaterialExpressionPanner, -1400, 400, speed_x=0.011, speed_y=0.006)
    connect(world_uv(m, "RippleTiling", 0.0095, -2200, 400), "", first_uv, "Coordinate")
    second_uv = node(m, unreal.MaterialExpressionPanner, -1400, 700, speed_x=-0.007, speed_y=0.009)
    connect(world_uv(m, "RippleTilingSecond", 0.0061, -2200, 700), "", second_uv, "Coordinate")
    first = picture(m, "Ripples", first_uv, -1100, 400, numbers=True)
    second = picture(m, "Ripples", second_uv, -1100, 700, numbers=True)
    lean = times(m, plus(m, leaning(m, first, "R", "G", -800, 400), leaning(m, second, "B", "A", -800, 700), -200, 550),
                 scalar(m, "RippleStrength", 0.07, -400, 800), 0, 550)
    up = node(m, unreal.MaterialExpressionConstant, 0, 750, r=1.0)
    normal = one_in(m, unreal.MaterialExpressionNormalize, pair(m, lean, up, 200, 550), 400, 550)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    to_property(scalar(m, "Roughness", 0.16, -400, 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.75, -400, 200), "", unreal.MaterialProperty.MP_SPECULAR)
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


# Each asset is left alone if it is already there, so adding one does not rewrite the others.
# Set TC_REBUILD=all to remake everything, or to a material's name to remake that one.
import os
REBUILD = os.environ.get("TC_REBUILD", "")
for name, build in (("M_MapSurface", build_map_surface), ("M_Solid", build_solid), ("M_Highlight", build_highlight), ("M_Flag", build_flag), ("M_Land", build_land), ("M_Sea", build_sea)):
    if REBUILD in ("all", name) or not eal.does_asset_exist(f"{MATERIALS}/{name}"):
        build()
    else:
        unreal.log(f"Theater Command: {MATERIALS}/{name} already exists, left alone")
build_level()
unreal.log("Theater Command: content built")
