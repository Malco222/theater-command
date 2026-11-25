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


def build_map_surface(name="M_MapSurface", soot=False, crowd=False, sway=False):
    """Land, sea and inked lines: the colour comes from the mesh itself, painted point by point in C++.
    The material adds what paint on a model has and a computer colour does not: a little unevenness and a slight tooth.
    (TK1: with soot, M_LandSteel, the far tanks', guns' and lorries' instanced pieces: the same surface, each point's
    paint taken toward "BurntColour" by its soot weight, the mesh's first texture coordinate's U, times how far her
    burning has gone, the instance's custom number 0, in the sRGB the mesh keeps, as the near meshes' colours are in C++.
    The burnt coat, release 2: where "BurntByPoint" is 1 the colour is each point's own, soot, rust or ash, its red in the
    first coordinate's V and its green and blue in the second coordinate (LandCrowd.cpp, PieceMesh); 0, the default, the
    one "BurntColour" as before.)
    (SO1: with crowd, M_LandCrowd, the ground battle's mid and far men as instances in the pose bank's poses: see
    crowd_poses.)
    (With sway, M_LandBlades, the ground battle's grass, corn, shrubs and trees that the wind bends: the same surface,
    leaning and springing with the gusts (wind_sway), and the gusts' sheen over its colour (wind_sheen). Brian, 23
    November: "can we make the grass, trees, and sky more visually realistic and epic?")"""
    m = new_material(name)
    # (the ground battle draws its hedges, trees and grass with this surface by instancing)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    # The mesh stores its colours the way a picture file does (sRGB); the renderer wants them linear.
    to_linear = node(m, unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    posed_facing = None
    if crowd:
        # SO1
        m.set_editor_property("tangent_space_normal", False)  # his posed facing is given in the world
        toned, posed_facing = crowd_poses(m, colour)
        connect(toned, "", to_linear, "Base")
    elif soot:
        # TK1
        first = node(m, unreal.MaterialExpressionTextureCoordinate, -1500, 150)
        weight = node_from(m, unreal.MaterialExpressionComponentMask, first, -1300, 150, r=True, g=False, b=False, a=False)
        burnt = one_in(m, unreal.MaterialExpressionSaturate, times(m, weight, node(m, unreal.MaterialExpressionPerInstanceCustomData, -1300, 250), -1150, 200), -1050, 200)
        own_red = node_from(m, unreal.MaterialExpressionComponentMask, first, -1300, 400, r=False, g=True, b=False, a=False)
        own = pair(m, own_red, node(m, unreal.MaterialExpressionTextureCoordinate, -1500, 500, coordinate_index=1), -1150, 450)
        to = lerp(m, vector(m, "BurntColour", (38.0 / 255.0, 33.0 / 255.0, 29.0 / 255.0, 1.0), -1150, 50), own, scalar(m, "BurntByPoint", 0.0, -1150, 600), -1000, 300)
        connect(lerp(m, colour, to, burnt, -850, 100), "", to_linear, "Base")
    else:
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
    # (and it can be greyed, nothing unless C++ says so: the pieces' paint by moonlight)
    weights = node(m, unreal.MaterialExpressionConstant3Vector, -450, -420, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
    brightness = node(m, unreal.MaterialExpressionDotProduct, -350, -420)
    connect(painted, "", brightness, "A")
    connect(weights, "", brightness, "B")
    greyed = lerp(m, painted, brightness, scalar(m, "Grey", 0.0, -350, -540), -300, -300)
    tinted = node(m, unreal.MaterialExpressionMultiply, -150, 0)
    connect(greyed, "", tinted, "A")
    connect(vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -450, -200), "", tinted, "B")
    # On ground nobody of the player's can see (art fix 11) the towns and the woods are drawn on the paper, not
    # modelled in colour: they lose about half of it. Where that ground lies is in the "Fog" picture, read by where
    # a point stands on the table ("FogFrame" turns the table's measure into the picture's); "Drawn" is nothing
    # unless C++ says so, so the armies and everything else keep their paint.
    where = node(m, unreal.MaterialExpressionWorldPosition, -1500, -900)
    north = node(m, unreal.MaterialExpressionComponentMask, -1300, -950, r=True, g=False, b=False, a=False)
    connect(where, "", north, "")
    east = node(m, unreal.MaterialExpressionComponentMask, -1300, -850, r=False, g=True, b=False, a=False)
    connect(where, "", east, "")
    frame = vector(m, "FogFrame", (0.0, 0.0, 0.0, 0.0), -1300, -1150)
    fog_u = plus(m, times(m, east, (frame, "R"), -1100, -850), (frame, "G"), -950, -850)
    fog_v = plus(m, times(m, north, (frame, "B"), -1100, -950), (frame, "A"), -950, -950)
    fog = picture(m, "Fog", pair(m, fog_u, fog_v, -800, -900), -650, -900, numbers=True)
    drawn = times(m, (fog, "R"), scalar(m, "Drawn", 0.0, -650, -700), -450, -800)
    weights2 = node(m, unreal.MaterialExpressionConstant3Vector, -450, -1000, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
    light2 = node(m, unreal.MaterialExpressionDotProduct, -300, -1000)
    connect(tinted, "", light2, "A")
    connect(weights2, "", light2, "B")
    # (The fog of war as a haze: what stands on unseen ground, a town or a wood, is in shadow with the ground it stands
    # on: two thirds as bright, half its colour gone, the same cold cast. It was drawn on paper there before.)
    sketched = times(m, vector(m, "DrawnTone", (0.92, 1.0, 1.07, 1.0), -300, -1150), lerp(m, tinted, light2, 0.5, -300, -1050), -150, -1050)
    tinted = lerp(m, tinted, times(m, sketched, scalar(m, "DrawnDark", 0.67, -150, -1150), 0, -950), drawn, 150, -500)
    # (The weather's sky lies on the towns and the woods as it lies on the ground they stand on; never on a piece or
    # a flag, whose materials never say "SkyOn".)
    tinted = sky_over(m, tinted, node_from(m, unreal.MaterialExpressionComponentMask, where, -1300, -2400, r=True, g=True, b=False, a=False), -1100, -5200, full=False)
    # The paint job of a piece (art fix 12): worn painted metal. "Wear" is nothing unless C++ says so (the pieces'
    # own material does). All paint is mottled a little, as paint is that has seen weather. Where the mesh marks
    # itself as painted metal (its alpha at a half: the tanks, guns, lorries, ships and aircraft), the paint is
    # chipped down to the bare metal in small patches, and the metal shines where the paint has gone. The marks are
    # laid out in the piece's own space, so they stay put on it when it is picked up and carried.
    wear = scalar(m, "Wear", 0.0, 300, -1500)
    # (A ship's wake and bow wave are paint laid on the water, which the mesh marks by an alpha of 240: no metal, and
    # by night it is dimmed with the sea by "WakeTone", which is white unless C++ says otherwise.)
    on_water = one_in(m, unreal.MaterialExpressionSaturate,
                      plus(m, times(m, one_in(m, unreal.MaterialExpressionAbs, plus(m, (colour, "A"), -0.9412, -1100, -1380), -1000, -1380), -60.0, -900, -1380), 1.0, -800, -1380), -700, -1380)
    metal = times(m, one_in(m, unreal.MaterialExpressionSaturate, times(m, one_in(m, unreal.MaterialExpressionOneMinus, (colour, "A"), -900, -1500), 2.0, -750, -1500), -600, -1500),
                  one_in(m, unreal.MaterialExpressionOneMinus, on_water, -600, -1420), -500, -1480)
    own = node(m, unreal.MaterialExpressionTransformPosition, -1500, -1700,
               transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
               transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(node(m, unreal.MaterialExpressionWorldPosition, -1750, -1700), "", own, "")
    own_xy = node(m, unreal.MaterialExpressionComponentMask, -1300, -1750, r=True, g=True, b=False, a=False)
    connect(own, "", own_xy, "")
    own_z = node(m, unreal.MaterialExpressionComponentMask, -1300, -1650, r=False, g=False, b=True, a=False)
    connect(own, "", own_z, "")
    skew = node(m, unreal.MaterialExpressionConstant2Vector, -1300, -1550, r=0.37, g=0.61)
    spread = plus(m, own_xy, times(m, own_z, skew, -1100, -1600), -950, -1700)
    chip_uv = times(m, spread, scalar(m, "ChipTiling", 0.035, -950, -1580), -800, -1700)
    blot_uv = times(m, spread, scalar(m, "BlotTiling", 0.009, -950, -1480), -800, -1560)
    speck = node(m, unreal.MaterialExpressionTextureSample, -600, -1750, texture=noise, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(chip_uv, "", speck, "UVs")
    blot = node(m, unreal.MaterialExpressionTextureSample, -600, -1560, texture=noise, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(blot_uv, "", blot, "UVs")
    # (a chip where the fine noise and the broad one are both high: chips come in patches, not evenly. They gather
    # where a vehicle is knocked about, its nose, its track guards and its hatches, which the mesh marks by taking
    # its alpha down from a half toward nothing: there the paint gives way far more readily.)
    gather = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, (colour, "A"), -1.0, -900, -1950), 0.5, -750, -1950), 2.56, -600, -1950), -450, -1950)
    chipped = times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate,
                                       times(m, plus(m, plus(m, times(m, (speck, "R"), plus(m, (blot, "G"), 0.5, -350, -1650), -200, -1700), scalar(m, "ChipAt", -0.78, -350, -1800), -50, -1750),
                                                     times(m, gather, scalar(m, "ChipGather", 0.3, -350, -1900), -200, -1900), 50, -1800),
                                             scalar(m, "ChipSharp", 9.0, -50, -1850), 150, -1750), 300, -1750), metal, 450, -1750), wear, 600, -1750)
    # (and what faces the sky is paler than what faces the side: the light from above, and the dust that settles)
    skyward = node(m, unreal.MaterialExpressionComponentMask, -900, -2100, r=False, g=False, b=True, a=False)
    connect(posed_facing if crowd else node(m, unreal.MaterialExpressionVertexNormalWS, -1100, -2100), "", skyward, "")
    lit_top = plus(m, 1.0, times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate, skyward, -750, -2100), scalar(m, "TopLight", 0.26, -750, -2000), -600, -2050),
                                 times(m, metal, wear, -600, -2150), -450, -2100), -300, -2100)
    weathered = times(m, times(m, tinted, lerp(m, 1.0, plus(m, times(m, (blot, "R"), 0.42, -350, -1500), 0.8, -200, -1500),
                                               times(m, wear, plus(m, times(m, metal, 0.5, 100, -1450), 0.5, 200, -1450), 300, -1450), 450, -1500), 600, -900), lit_top, 700, -1000)
    tinted = lerp(m, weathered, vector(m, "ChipColour", (0.13, 0.125, 0.12, 1.0), 450, -1300), chipped, 750, -900)
    tinted = times(m, tinted, lerp(m, 1.0, vector(m, "WakeTone", (1.0, 1.0, 1.0, 1.0), 600, -1250), on_water, 800, -1200), 900, -950)
    if sway:
        tinted = times(m, tinted, wind_sheen(m, -900, 3200), 1000, -950)
        to_property(wind_sway(m, -900, 3600), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    to_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_property(times(m, chipped, 0.85, 750, -700), "", unreal.MaterialProperty.MP_METALLIC)
    # (The art director, 19 November 2026: a rifleman seen with the sun behind him stood darker on his shaded side
    # than the shadow he threw on the ground. What is not painted metal, the cloth and the flesh of a figure, gives
    # back a little of its own colour on the side turned from the sun: "FigureFill". And the edge of it that is
    # turned to the sun catches the light when the sun stands behind it: a thin rim, whose strength is
    # "FigureSun"'s alpha. The way to the sun is "FigureSun"'s red, green and blue. Both are nothing unless C++
    # says so, which only the pieces' material does: the land, the towns and the woods are as they were. No light
    # is added to the scene and nothing throws a new shadow.)
    figure_sun = vector(m, "FigureSun", (0.0, 0.0, 1.0, 0.0), 900, -2600)
    facing = posed_facing if crowd else node(m, unreal.MaterialExpressionVertexNormalWS, 900, -2400)
    to_sun = node(m, unreal.MaterialExpressionDotProduct, 1100, -2500)
    connect(facing, "", to_sun, "A")
    connect(figure_sun, "", to_sun, "B")
    eye_way = node(m, unreal.MaterialExpressionCameraVectorWS, 900, -2250)
    to_eye = node(m, unreal.MaterialExpressionDotProduct, 1100, -2300)
    connect(facing, "", to_eye, "A")
    connect(eye_way, "", to_eye, "B")
    sun_behind = node(m, unreal.MaterialExpressionDotProduct, 1100, -2150)
    connect(eye_way, "", sun_behind, "A")
    connect(figure_sun, "", sun_behind, "B")
    away = one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, to_sun, -3.0, 1300, -2500), 0.4, 1450, -2500), 1600, -2500)
    edge = one_in(m, unreal.MaterialExpressionOneMinus, one_in(m, unreal.MaterialExpressionSaturate, to_eye, 1300, -2300), 1450, -2300)
    edge = times(m, times(m, edge, edge, 1600, -2300), edge, 1750, -2300)
    rim = times(m, times(m, edge, one_in(m, unreal.MaterialExpressionSaturate, to_sun, 1300, -2400), 1900, -2350),
                times(m, one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, sun_behind, -1.0, 1300, -2150), 0.3, 1450, -2150), 1600, -2150), (figure_sun, "A"), 1750, -2150), 2050, -2250)
    cloth = times(m, one_in(m, unreal.MaterialExpressionOneMinus, metal, 1300, -2700), one_in(m, unreal.MaterialExpressionOneMinus, on_water, 1300, -2800), 1450, -2750)
    given = times(m, times(m, tinted, cloth, 2050, -2600), plus(m, times(m, away, scalar(m, "FigureFill", 0.0, 1600, -2650), 1750, -2600), rim, 2200, -2450), 2350, -2500)
    to_property(given, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Tooth: the engine's paper bump, laid flat and faded in by a strength.
    paper = unreal.load_asset("/Engine/EngineMaterials/PaperNormal")
    bump = node(m, unreal.MaterialExpressionTextureSample, -1100, 950, texture=paper, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    connect(world_uv(m, "ToothTiling", 0.021, -1800, 950), "", bump, "UVs")
    flat = node(m, unreal.MaterialExpressionConstant3Vector, -1100, 1200, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    tooth = node(m, unreal.MaterialExpressionLinearInterpolate, -650, 1000)
    connect(flat, "", tooth, "A")
    connect(bump, "RGB", tooth, "B")
    connect(scalar(m, "Tooth", 0.35, -850, 1250), "", tooth, "Alpha")
    # (a posed man: his facing as posed, in the world; the men's ink has no tooth)
    to_property(posed_facing if crowd else tooth, "", unreal.MaterialProperty.MP_NORMAL)

    # (bare metal is smoother than paint: the lamp glints on the chips)
    to_property(lerp(m, scalar(m, "Roughness", 0.9, -450, 200), 0.34, chipped, -250, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
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


def build_cloud():
    """THE AIR BATTLES' CLOUDS (the art director's brief, item 4, 20 November 2026: torn fragments with crisp sunlit
    rims in place of soft discs). Each puff is one shape from the sheet the game paints when a battle opens
    ("Sheet", AirSky::MakeFragments): its first texture coordinate is the place on the sheet, its second how far the
    sun is behind the eye (the first part) or ahead of it (the second). The sheet holds plain numbers, each half the
    share of the cloud's light: red with the sun to one side, green with it behind the eye, blue with it ahead (thin
    edges bright), alpha how much cloud there is. The mesh's colour is the cloud's own paint, its alpha how much of
    the puff is shown. Light of its own, see-through, seen from both sides."""
    m = new_material("M_Cloud")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    sheet = picture(m, "Sheet", None, -1100, 0, numbers=True)
    sun = node(m, unreal.MaterialExpressionTextureCoordinate, -1300, 400, coordinate_index=1)
    behind = node_from(m, unreal.MaterialExpressionComponentMask, sun, -1100, 400, r=True, g=False, b=False, a=False)
    ahead = node_from(m, unreal.MaterialExpressionComponentMask, sun, -1100, 520, r=False, g=True, b=False, a=False)

    def less(a, b, x, y):
        n = node(m, unreal.MaterialExpressionSubtract, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    # light = 2 (side + (front - side) behind + (back - side) ahead)
    toward = times(m, less((sheet, "G"), (sheet, "R"), -850, 200), behind, -650, 250)
    against = times(m, less((sheet, "B"), (sheet, "R"), -850, 450), ahead, -650, 450)
    light = times(m, plus(m, plus(m, (sheet, "R"), toward, -450, 150), against, -300, 250), 2.0, -150, 250)
    colour = node(m, unreal.MaterialExpressionVertexColor, -700, -300)
    lit = times(m, times(m, colour, light, -150, -200), scalar(m, "Brightness", 1.3, -350, -100), 50, -200)
    tinted = times(m, lit, vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -150, -400), 250, -250)
    to_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    shown = times(m, times(m, (colour, "A"), (sheet, "A"), -450, 650), scalar(m, "Opacity", 1.0, -450, 780), -250, 650)
    to_property(one_in(m, unreal.MaterialExpressionSaturate, shown, -50, 650), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def _sky_chip_code():
    """GS6: M_CloudTurned's Custom node: Battle/AirSkyChip.inl as it stands (the clouds' arithmetic, which the cloud test
    also compiles as C++ and checks against the painter's), then a few lines that read one puff's texels from the
    table and hand back where the point goes (the mesh's own units, its own frame) with its colour and its place on
    the sheet."""
    import os
    places = []
    try:
        places.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "TheaterCommand", "Battle", "AirSkyChip.inl"))
    except NameError:  # (run where the script's own path is not given)
        pass
    places.append(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Source", "TheaterCommand", "Battle", "AirSkyChip.inl"))
    for path in places:
        if os.path.isfile(path):
            with open(path, encoding="utf-8") as f:
                maths = f.read()
            break
    else:
        raise RuntimeError("Battle/AirSkyChip.inl was not found at " + " or ".join(places))
    texels = ", ".join(f"SkyTable.Load(int3({k}, 0, 0))" for k in range(5)) + ",\n    " + \
        ", ".join(f"SkyTable.Load(int3(SkyCol + {k}, SkyRow, 0))" for k in range(8))
    return maths + f"""
// (the glue: one point of one puff, for the eye where it is this frame)
FSkyChip SkyChip;
SkyChip.Pad = 0.0f;
int SkyCol = (int)(SkySlot.x + 0.5f);
int SkyRow = (int)(SkySlot.y + 0.5f);
float3 SkyLocal = SkyEye / SkyUnits;
FSkyChipOut SkyOut = SkyChip.Vertex({texels},
    float3(SkyLocal.y, SkyLocal.x, SkyLocal.z), SkyCol / 8 + (SkyRow - 1) * 256, (int)(SkyRing.x + 0.5f), (int)(SkyRing.y + 0.5f));
SkyColour = SkyOut.Colour;
SkySheet = SkyOut.Sheet;
return float3(SkyOut.At.y, SkyOut.At.x, SkyOut.At.z) * SkyUnits;
"""


def build_cloud_turned():
    """GS6, THE CLOUDS TURNED ON THE GRAPHICS CHIP (the speed gate's sky: the same clouds as M_Cloud, with nothing turned
    or sent from the game's thread as the eye moves). One mesh, made once, a puff's room for each slot of the table:
    each point's first texture coordinate is its place round its puff (the point round the ring, and which ring: 0 the
    middle, 1 the heart, 2 the rim), its second its slot's first texel in "Table" (a float picture the game lays, far
    to near: AirSky::Chip). Where the points are placed (the vertex shader) the Custom node runs Battle/AirSkyChip.inl,
    the painter's own arithmetic, for the eye as it is this frame ("UnitsPerMetre": the scene's), and the point is
    moved there; its colour and its place on the sheet are carried to the pixels, which are M_Cloud's (the "Sheet" of
    shapes, "Brightness", "Tint", "Opacity"). Light of its own, see-through, seen from both sides."""
    m = new_material("M_CloudTurned")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)

    chip = node(m, unreal.MaterialExpressionCustom, -2400, 900)
    chip.set_editor_property("description", "AirSkyChip")
    chip.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    chip.set_editor_property("code", _sky_chip_code())
    inputs = []
    for name in ("SkyTable", "SkyRing", "SkySlot", "SkyEye", "SkyUnits"):
        each = unreal.CustomInput()
        each.set_editor_property("input_name", name)
        inputs.append(each)
    chip.set_editor_property("inputs", inputs)
    outputs = []
    for name in ("SkyColour", "SkySheet"):
        each = unreal.CustomOutput()
        each.set_editor_property("output_name", name)
        each.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        outputs.append(each)
    chip.set_editor_property("additional_outputs", outputs)

    stand = stand_in(True)
    table = node(m, unreal.MaterialExpressionTextureObjectParameter, -2900, 700, parameter_name="Table", texture=stand, sampler_type=sampler_for(stand))
    connect(table, "", chip, "SkyTable")
    connect(node(m, unreal.MaterialExpressionTextureCoordinate, -2900, 850, coordinate_index=0), "", chip, "SkyRing")
    connect(node(m, unreal.MaterialExpressionTextureCoordinate, -2900, 950, coordinate_index=1), "", chip, "SkySlot")
    eye = node(m, unreal.MaterialExpressionTransformPosition, -2700, 1050,
               transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
               transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(node(m, unreal.MaterialExpressionCameraPositionWS, -2950, 1050), "", eye, "")
    connect(eye, "", chip, "SkyEye")
    connect(scalar(m, "UnitsPerMetre", 100.0, -2900, 1150), "", chip, "SkyUnits")

    # The point moved from where the mesh made it to where the chip puts it, in the world.
    away = node(m, unreal.MaterialExpressionSubtract, -2100, 900)
    connect(chip, "", away, "A")
    connect(node(m, unreal.MaterialExpressionPreSkinnedPosition, -2300, 1050), "", away, "B")
    moved = node(m, unreal.MaterialExpressionTransform, -1900, 900,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    connect(away, "", moved, "")
    to_property(moved, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # The pixels: M_Cloud's, from the colour and the sheet's place carried from the points.
    colour = one_in(m, unreal.MaterialExpressionVertexInterpolator, (chip, "SkyColour"), -1700, -300)
    carried = one_in(m, unreal.MaterialExpressionVertexInterpolator, (chip, "SkySheet"), -1700, 300)
    paint = node_from(m, unreal.MaterialExpressionComponentMask, colour, -1500, -300, r=True, g=True, b=True, a=False)
    alpha = node_from(m, unreal.MaterialExpressionComponentMask, colour, -1500, 650, r=False, g=False, b=False, a=True)
    place = node_from(m, unreal.MaterialExpressionComponentMask, carried, -1400, 0, r=True, g=True, b=False, a=False)
    sheet = picture(m, "Sheet", place, -1100, 0, numbers=True)
    behind = node_from(m, unreal.MaterialExpressionComponentMask, carried, -1100, 400, r=False, g=False, b=True, a=False)
    ahead = node_from(m, unreal.MaterialExpressionComponentMask, carried, -1100, 520, r=False, g=False, b=False, a=True)

    def less(a, b, x, y):
        n = node(m, unreal.MaterialExpressionSubtract, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    # light = 2 (side + (front - side) behind + (back - side) ahead), as M_Cloud
    toward = times(m, less((sheet, "G"), (sheet, "R"), -850, 200), behind, -650, 250)
    against = times(m, less((sheet, "B"), (sheet, "R"), -850, 450), ahead, -650, 450)
    light = times(m, plus(m, plus(m, (sheet, "R"), toward, -450, 150), against, -300, 250), 2.0, -150, 250)
    lit = times(m, times(m, paint, light, -150, -200), scalar(m, "Brightness", 1.3, -350, -100), 50, -200)
    tinted = times(m, lit, vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -150, -400), 250, -250)
    to_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    shown = times(m, times(m, alpha, (sheet, "A"), -450, 650), scalar(m, "Opacity", 1.0, -450, 780), -250, 650)
    to_property(one_in(m, unreal.MaterialExpressionSaturate, shown, -50, 650), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    try:  # (what the points' shader costs, for the next Mac run's notes; an engine without the call says nothing)
        stats = mel.get_statistics(m)
        unreal.log(f"Theater Command: M_CloudTurned: {stats.get_editor_property('num_vertex_shader_instructions')} vertex shader instructions, "
                   f"{stats.get_editor_property('num_pixel_shader_instructions')} pixel")
    except Exception:
        pass


def build_cloud_turned_or_tell():
    """GS6: M_CloudTurned is the clouds' on the graphics chip only; without it the scenes turn them on the game's thread
    as before (LandBattle's LaySky says so). Whatever goes wrong in it is told, and the rest is built as before."""
    import traceback
    try:
        build_cloud_turned()
    except Exception:
        unreal.log_error("Theater Command: M_CloudTurned was not built (the clouds are turned on the game's thread, as before):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_CloudTurned"):
                eal.delete_asset(f"{MATERIALS}/M_CloudTurned")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_CloudTurned could not be removed:\n" + traceback.format_exc())


def build_wake():
    """The foam of a ship's wake, lying on the water: light of its own, see-through. The mesh's own colour says how
    white the foam is (red), how whole it still is (green: 1 unbroken by the stern, falling as it breaks up) and how
    much of it there is (alpha: nothing at a soft edge). Its texture coordinate runs astern (u) and across (v) in
    ship's beams. Where the foam is no longer whole, only the heights of a noise keep it: drawn out along the ship's
    track they are streaks, and they drift slowly astern."""
    m = new_material("M_Wake")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    noise = unreal.load_asset("/Engine/MapTemplates/TilingNoise05")
    colour = node(m, unreal.MaterialExpressionVertexColor, -1500, 0)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1900, 400)
    drift = scalar(m, "Drift", 0.022, -1900, 620)

    def streaks(tile_u, tile_v, speed, x, y):
        scaled = times(m, uv, node(m, unreal.MaterialExpressionConstant2Vector, x - 420, y + 120, r=tile_u, g=tile_v), x - 250, y)
        moved = node(m, unreal.MaterialExpressionPanner, x - 100, y, speed_x=-1.0, speed_y=0.0)
        connect(scaled, "", moved, "Coordinate")
        clock = times(m, node(m, unreal.MaterialExpressionTime, x - 420, y + 240), times(m, drift, speed, x - 420, y + 320), x - 250, y + 240)
        connect(clock, "", moved, "Time")
        # (The noise is drawn out fifteen times longer than it is wide. Left to itself the sampler takes it from a
        # picture small enough for the long way, which is a flat grey: no heights, no streaks. It is told to take a
        # picture nearer the size of the short way.)
        sample = node(m, unreal.MaterialExpressionTextureSample, x + 80, y, texture=noise, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                      mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_BIAS, const_mip_value=-3.4)
        connect(moved, "", sample, "UVs")
        return sample

    # (two scales of streak: long thin ones, and a broader breaking-up under them that drifts a little slower)
    fine = streaks(0.11, 1.7, 1.0, -1400, 300)
    broad = streaks(0.045, 0.55, 0.7, -1400, 700)
    mix = plus(m, times(m, (fine, "R"), 0.62, -1000, 300), times(m, (broad, "G"), 0.38, -1000, 700), -850, 500)
    # (stretched about its middle, so that the heights and hollows of the noise reach both ends)
    # (the noise is a dark one, with its middle near a fifth: "Centre" is where that middle is taken to be)
    mix = plus(m, times(m, plus(m, mix, times(m, scalar(m, "Centre", 0.23, -900, 560), -1.0, -820, 560), -750, 500), scalar(m, "Contrast", 2.4, -900, 640), -650, 500), 0.5, -550, 500)
    kept = one_in(m, unreal.MaterialExpressionSaturate,
                  times(m, plus(m, plus(m, mix, -1.0, -450, 500), times(m, (colour, "G"), 1.6, -650, 300), -350, 420), scalar(m, "Sharp", 3.0, -500, 640), -250, 460), -120, 460)
    # (a point that says it is of the band under the foam, in its blue, takes the band's colour: the sea's own, lifted and a touch greener)
    foam = times(m, times(m, times(m, lerp(m, vector(m, "Foam", (0.6, 0.66, 0.68, 1.0), -900, -260), vector(m, "Band", (0.3, 0.62, 0.58, 1.0), -900, -380), (colour, "B"), -780, -300), (colour, "R"), -650, -200),
                          scalar(m, "Brightness", 1.0, -900, -80), -450, -160),
                 vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -650, -20), -250, -120)
    to_property(foam, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # (a wake stops at the west edge of the map: a fleet near it trailed its foam out over the table's frame)
    east = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionWorldPosition, -500, 820), -300, 820, r=False, g=True, b=False, a=False)
    on_the_map = one_in(m, unreal.MaterialExpressionSaturate,
                        times(m, plus(m, east, times(m, scalar(m, "WestEdge", -10000000.0, -500, 940), -1.0, -400, 940), -200, 860), scalar(m, "EdgeSoft", 0.01, -400, 1020), -100, 900), 0, 900)
    to_property(times(m, times(m, times(m, kept, (colour, "A"), 0, 380), scalar(m, "Opacity", 1.0, -200, 640), 150, 420), on_the_map, 300, 500), "", unreal.MaterialProperty.MP_OPACITY)
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
    # (A battered army's flag is dirtied, a very little: its colours are drawn toward a dark dun by "Fade". It is not
    # bleached: a faded black-white-red read as Poland's white and red. Its wear is told by its shape, and by soot,
    # which the cloth's own points carry as their colour: white where the cloth is clean.)
    cloth = lerp(m, (picture, "RGB"), vector(m, "Faded", (0.1, 0.09, 0.07, 1.0), -700, 180), scalar(m, "Fade", 0.0, -700, 260), -450, 60)
    # (Ruling 154; the art director's review of the haze.) The flag of an army only glimpsed through the haze: a
    # quarter of its colour is drained ("Drain") and a tenth of the haze's own colour lies over it ("Veiled", toward
    # "Veil"). Her rule is worked on the colours as they are seen, so the cloth is taken to the screen's own measure
    # for it and back again.
    seen = node(m, unreal.MaterialExpressionPower, -450, 600, const_exponent=0.4545)
    connect(cloth, "", seen, "Base")
    seen_weights = node(m, unreal.MaterialExpressionConstant3Vector, -450, 760, constant=unreal.LinearColor(0.299, 0.587, 0.114, 0.0))
    seen_grey = node(m, unreal.MaterialExpressionDotProduct, -300, 700)
    connect(seen, "", seen_grey, "A")
    connect(seen_weights, "", seen_grey, "B")
    veiled = lerp(m, lerp(m, seen, seen_grey, scalar(m, "Drain", 0.0, -300, 860), -150, 650),
                  vector(m, "Veil", (0.47, 0.5, 0.53, 1.0), -150, 860), scalar(m, "Veiled", 0.0, -150, 960), 0, 700)
    unseen = node(m, unreal.MaterialExpressionPower, 150, 700, const_exponent=2.2)
    connect(veiled, "", unseen, "Base")
    cloth = unseen
    cloth = times(m, cloth, node(m, unreal.MaterialExpressionVertexColor, -700, -150), -300, 0)
    to_property(cloth, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionMultiply, -350, 300)
    connect(cloth, "", glow, "A")
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


def node_from(material, kind, source, x, y, **properties):
    """A node with one input, fed from another node: a mask, for the most part."""
    n = node(material, kind, x, y, **properties)
    connect(*(source if isinstance(source, tuple) else (source, "")), n, "")
    return n


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


def over(material, a, b, x=0, y=0):
    n = node(material, unreal.MaterialExpressionDivide, x, y)
    _feed(material, n, "A", a)
    _feed(material, n, "B", b)
    return n


def crowd_poses(m, colour):
    """THE MEN AS ONE CROWD (the true-scale plan's SO1, the vertex-animation form; the soldiers thread's bake, LandMen's
    BankBody). Each body set of men alike out there is one mesh at rest, drawn by instancing; every pose of the bank is
    in one picture handed in by C++ ("Poses", plain numbers kept as half floats, read texel by texel): a row a pose, a
    column a point of the mesh, each point's way from its rest (Unreal's units, his own axes) in the first "PoseCount"
    rows and its facing in the next as many. The mesh's second texture coordinate's U is the point's number (a whole
    number, which a half float holds exactly up to 2048: C++ keeps a set under that), "Columns" the picture's width,
    "Rows" its height (two rows a pose), "PoseCount" how many.

    The instance's own numbers say what he shows (LandMen's BankShow): 0 and 1 the two poses and 2 the share of the
    second (so he goes smoothly between them), 3 to 5 the same a frame before (BankShowBefore, for the anti-aliasing's
    sense of movement), and 6 to 8 his face's tone, which multiplies his skin's colour (the mesh's first texture
    coordinate's U is 1 on skin) in the sRGB the mesh keeps, as LandMen's MakeLook does for a skinned man.

    All of it is read where the points are placed (the vertex shader), at the picture's first level; the facing and the
    toned colour are carried to the pixels. Returns the toned colour (sRGB, to be made linear as any other) and his
    facing in the world, unit length, in place of the mesh's."""
    stand = stand_in(True)
    columns = scalar(m, "Columns", 1.0, -4200, 1600)
    rows = scalar(m, "Rows", 2.0, -4200, 1700)
    poses = scalar(m, "PoseCount", 1.0, -4200, 1800)
    point = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionTextureCoordinate, -4400, 1500, coordinate_index=1), -4200, 1500,
                      r=True, g=False, b=False, a=False)
    u = over(m, plus(m, point, 0.5, -4000, 1500), columns, -3850, 1550)
    numbers = {}

    def own(index, default=0.0):
        if index not in numbers:
            n = node(m, unreal.MaterialExpressionPerInstanceCustomData, -4200, 2000 + 100 * index, data_index=index)
            if default != 0.0:
                try:
                    n.set_editor_property("const_default_value", default)
                except Exception:  # (an engine that names it otherwise: his tone is always handed in)
                    pass
            numbers[index] = n
        return numbers[index]

    def read(row, x, y):
        """The picture's numbers at this point in row 'row' (a node)."""
        v = over(m, plus(m, row, 0.5, x - 300, y), rows, x - 150, y)
        n = node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name="Poses", texture=stand, sampler_type=sampler_for(stand),
                 mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=0)
        connect(pair(m, u, v, x - 100, y + 60), "", n, "UVs")
        return (n, "RGB")

    def to_world(v, x, y):
        n = node(m, unreal.MaterialExpressionTransform, x, y,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        connect(*(v if isinstance(v, tuple) else (v, "")), n, "")
        return n

    def moved(first, y):
        """His points' way from rest, in the world, by the own numbers first (pose A), first + 1 (pose B), first + 2 (B's share)."""
        a = read(own(first), -3500, y)
        b = read(own(first + 1), -3500, y + 150)
        return to_world(lerp(m, a, b, own(first + 2), -3200, y + 75), -3000, y + 75)

    now = moved(0, 2000)
    before = moved(3, 2400)
    # (the frame before, where the anti-aliasing asks: an engine without the switch, or naming its pins otherwise,
    # takes this frame's for both, said in the log)
    offset = now
    try:
        switch = node(m, unreal.MaterialExpressionPreviousFrameSwitch, -2700, 2200)
        for current_pin, previous_pin in (("Current Frame", "Previous Frame"), ("CurrentFrame", "PreviousFrame")):
            if mel.connect_material_expressions(now, "", switch, current_pin) and mel.connect_material_expressions(before, "", switch, previous_pin):
                offset = switch
                break
        if offset is not switch:
            unreal.log_warning("Theater Command: M_LandCrowd: the frame-before switch's pins were not found; the men's poses give the anti-aliasing this frame's only")
    except Exception as e:
        unreal.log_warning(f"Theater Command: M_LandCrowd: no frame-before switch ({e}); the men's poses give the anti-aliasing this frame's only")
    to_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # His facing as posed (the rows after the positions), turned into the world, carried to the pixels and made unit long.
    fa = read(plus(m, own(0), poses, -3800, 2800), -3500, 2800)
    fb = read(plus(m, own(1), poses, -3800, 2950), -3500, 2950)
    facing = to_world(lerp(m, fa, fb, own(2), -3200, 2875), -3000, 2875)
    facing = one_in(m, unreal.MaterialExpressionNormalize, one_in(m, unreal.MaterialExpressionVertexInterpolator, facing, -2800, 2875), -2600, 2875)

    # His face's tone on his skin, worked where the points are and carried to the pixels.
    skin = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionTextureCoordinate, -4400, 3200, coordinate_index=0), -4200, 3200,
                     r=True, g=False, b=False, a=False)
    tone = pair(m, pair(m, own(6, 1.0), own(7, 1.0), -4000, 3300), own(8, 1.0), -3850, 3300)
    toned = one_in(m, unreal.MaterialExpressionVertexInterpolator, lerp(m, colour, times(m, colour, tone, -3600, 3300), skin, -3400, 3250), -3200, 3250)
    return toned, facing


def build_land_crowd_or_tell():
    """SO1: M_LandCrowd (M_MapSurface's paint on the pose bank's men drawn as instances: crowd_poses) is the ground
    battle's alone, and the battle falls back to a mesh a set and pose without it (LandBattle's BankCopyFor; mid men
    then skinned as before): whatever goes wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_map_surface("M_LandCrowd", crowd=True)
    except Exception:
        unreal.log_error("Theater Command: M_LandCrowd was not built (far men are drawn a mesh a pose, mid men skinned):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandCrowd"):
                eal.delete_asset(f"{MATERIALS}/M_LandCrowd")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandCrowd could not be removed:\n" + traceback.format_exc())


def sky_over(m, c, place, x, y, full=True):
    """THE WEATHER'S SKY (Brian, 16 November 2026: "weather and cloud cover would be interesting parts to. if the
    weather is bad then you cant do bombing raids and airplanes are grounded"; ruling 127; the art director's
    weather-sky brief). Each month every weather zone is clear, in cloud, or closed. Most of the time he sees only
    the weather's light on the ground: under cloud, soft shadows drift over about two fifths of it; under a closed
    sky the whole zone sits under an even lid, dimmer, cooler, a third of its colour drained, with thin flat bands of
    mist, and rain. In the sky view ("SkyView", nothing at rest) the cloud itself is drawn flat on the map: white
    broken cloud up and left of its shadows, or one pale grey sheet over a closed zone.

    Where each sky lies is in the "Sky" picture, read by where a point stands on the table ("SkyFrame"): its red is
    cloud and its green a closed sky, soft at the zones' edges. The shapes come from one picture that repeats,
    "Weather": red soft cloud shapes, green the billow inside a cloud, blue long flat bands of mist, alpha thin
    streaks of rain (or flakes of snow). It drifts east by the game's own clock ("SkyTravel"). All of it is nothing
    until the game says "SkyOn": a flag, a plate and a piece never wear it."""
    east = node_from(m, unreal.MaterialExpressionComponentMask, place, x, y, r=False, g=True, b=False, a=False)
    north = node_from(m, unreal.MaterialExpressionComponentMask, place, x, y + 120, r=True, g=False, b=False, a=False)
    frame = vector(m, "SkyFrame", (0.0, 0.0, 0.0, 0.0), x, y + 260)
    sky = picture(m, "Sky", pair(m, plus(m, times(m, east, (frame, "R"), x + 200, y), (frame, "G"), x + 350, y),
                                 plus(m, times(m, north, (frame, "B"), x + 200, y + 120), (frame, "A"), x + 350, y + 120), x + 500, y), x + 650, y, numbers=True)
    on = scalar(m, "SkyOn", 0.0, x + 650, y + 260)
    cloud = times(m, (sky, "R"), on, x + 850, y)
    closed = times(m, (sky, "G"), on, x + 850, y + 120)
    view = scalar(m, "SkyView", 0.0, x + 850, y + 260)
    upwind = plus(m, east, times(m, scalar(m, "SkyTravel", 0.0, x, y + 420), -1.0, x + 100, y + 420), x + 250, y + 420)

    def weather(wide, high, off_east, off_north, xx, yy):
        return picture(m, "Weather", pair(m, times(m, plus(m, upwind, off_east, xx, yy), 1.0 / wide, xx + 150, yy),
                                          times(m, plus(m, north, off_north, xx, yy + 120), 1.0 / high, xx + 150, yy + 120), xx + 300, yy), xx + 450, yy, numbers=True)

    def grey_of(v, xx, yy):
        weights = node(m, unreal.MaterialExpressionConstant3Vector, xx, yy, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
        grey = node(m, unreal.MaterialExpressionDotProduct, xx + 150, yy)
        _feed(m, grey, "A", v)
        connect(weights, "", grey, "B")
        return grey

    # Cloud, at rest: shadows about 75 km across, wider than tall, soft edged; under one the ground is darker and a
    # little drained.
    shapes = weather(2520.0, 1800.0, 0.0, 0.0, x, y + 600)
    shaded = times(m, lerp(m, c, grey_of(c, x + 700, y + 760), 0.12, x + 900, y + 700), scalar(m, "SkyShadeDim", 0.72, x + 900, y + 820), x + 1050, y + 700)
    c = lerp(m, c, shaded, times(m, (shapes, "R"), cloud, x + 700, y + 600), x + 1200, y + 600)
    # Closed, at rest: the lid.
    lid = times(m, times(m, lerp(m, c, grey_of(c, x + 700, y + 1060), scalar(m, "SkyDrain", 0.32, x + 700, y + 1160), x + 900, y + 1000),
                         vector(m, "SkyCast", (0.89, 0.977, 1.117, 1.0), x + 900, y + 1120), x + 1050, y + 1000), scalar(m, "SkyDim", 0.57, x + 1050, y + 1120), x + 1200, y + 1000)
    if full:
        mist = weather(5200.0, 600.0, 300.0, 170.0, x, y + 1350)
        lid = lerp(m, lid, vector(m, "SkyMist", (0.617, 0.658, 0.701, 1.0), x + 700, y + 1480),
                   times(m, (mist, "B"), scalar(m, "SkyMists", 0.02, x + 700, y + 1580), x + 900, y + 1400), x + 1350, y + 1200)
    c = lerp(m, c, lid, closed, x + 1500, y + 900)
    # The sky view. Cloud: puffs up and left of their shadows, with a soft billow inside, and clear ground between.
    puffs = weather(2520.0, 1800.0, 45.0, -45.0, x, y + 1800)
    much = times(m, times(m, (puffs, "R"), plus(m, times(m, (puffs, "G"), 0.5, x + 500, y + 1900), 0.75, x + 600, y + 1900), x + 700, y + 1800),
                 scalar(m, "SkyPuffs", 0.12, x + 700, y + 1950), x + 850, y + 1800)
    c = lerp(m, c, vector(m, "SkyPuff", (0.871, 0.896, 0.93, 1.0), x + 850, y + 2000), times(m, much, times(m, cloud, view, x + 850, y + 2120), x + 1000, y + 1900), x + 1650, y + 1500)
    # Closed: one pale grey sheet, a little thicker here and thinner there so that it is not flat.
    sheet = times(m, plus(m, times(m, (shapes, "R"), 0.25, x + 700, y + 2300), 0.75, x + 800, y + 2300), scalar(m, "SkySheets", 0.095, x + 800, y + 2420), x + 950, y + 2300)
    c = lerp(m, c, vector(m, "SkySheet", (0.617, 0.658, 0.73, 1.0), x + 950, y + 2500), times(m, sheet, times(m, closed, view, x + 950, y + 2620), x + 1100, y + 2400), x + 1800, y + 1900)
    if full:
        # Rain (snow in the winter months) under a closed sky: one cheap layer across the screen, laid on the ground
        # and the sea only. "RainTiles" is how many of the picture the view is across and down; "RainFall" is how far
        # it has fallen, by the game's clock.
        screen = node(m, unreal.MaterialExpressionScreenPosition, x, y + 2800)
        tiles = node_from(m, unreal.MaterialExpressionComponentMask, vector(m, "RainTiles", (3.125, 1.953, 0.0, 0.0), x, y + 2950), x + 200, y + 2950, r=True, g=True, b=False, a=False)
        fall = node_from(m, unreal.MaterialExpressionComponentMask, vector(m, "RainFall", (0.0, 0.0, 0.0, 0.0), x, y + 3100), x + 200, y + 3100, r=True, g=True, b=False, a=False)
        drops = picture(m, "Weather", plus(m, times(m, screen, tiles, x + 400, y + 2850), fall, x + 550, y + 2900), x + 700, y + 2850, numbers=True)
        c = lerp(m, c, vector(m, "SkyRainInk", (0.776, 0.823, 0.888, 1.0), x + 900, y + 3000),
                 times(m, (drops, "A"), times(m, closed, scalar(m, "SkyRain", 0.2, x + 900, y + 3120), x + 1050, y + 3050), x + 1200, y + 2900), x + 1950, y + 2400)
    return c


def build_land():
    """The land. Its colour comes from pictures the game paints when it starts (CampaignGround.cpp), one pixel
    for every point of the terrain: the bare ground, the nations' paint, the lie of the land and where rock,
    snow and sand show through. Over that goes fine detail that repeats: grass, tree-tops, rock, sand."""
    m = new_material("M_Land")
    m.set_editor_property("tangent_space_normal", False)  # the picture says which way the ground leans in the world
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -3000, 0)
    tint = picture(m, "Tint", uv, -2600, -250)
    # The ownership paint (the art director's brief of 15 November 2026). Occupied ground keeps soft pinstripes of
    # its lawful owner's colour under its holder's: woven into the stain, not ruled over it. They fall to the right,
    # at right angles to the fog's ruled lines, sixteen pixels apart at working height (0.38 of the fog's spacing)
    # and four tenths of that wide, each edge fading over a quarter of the stripe's width. The picture says where
    # the ground is occupied and what colour a stripe takes there (the holder's stain mixed toward the owner's).
    # From higher, as the stripes close to less than eight pixels apart, they go into their flat average, and
    # below five they are that alone: so nothing shimmers. None on fogged ground.
    occupied = picture(m, "Occupied", uv, -2600, -3000)
    stripe_place = world_uv(m, "StripeTiling", 0.0158, -3000, -3300)
    stripe_one = node_from(m, unreal.MaterialExpressionComponentMask, stripe_place, -2400, -3300, r=True, g=False, b=False, a=False)
    stripe_other = node_from(m, unreal.MaterialExpressionComponentMask, stripe_place, -2400, -3220, r=False, g=True, b=False, a=False)
    stripe_along = plus(m, stripe_one, times(m, stripe_other, -1.0, -2250, -3220), -2100, -3260)
    stripe_off = one_in(m, unreal.MaterialExpressionAbs, plus(m, one_in(m, unreal.MaterialExpressionFrac, stripe_along, -1950, -3260), -0.5, -1800, -3260), -1650, -3260)
    stripe = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, stripe_off, -1.0, -1500, -3260), 0.25, -1350, -3260), 10.0, -1200, -3260), -1050, -3260)
    stripe_closeness = plus(m, one_in(m, unreal.MaterialExpressionAbs, one_in(m, unreal.MaterialExpressionDDX, stripe_along, -1950, -3400), -1800, -3400),
                            one_in(m, unreal.MaterialExpressionAbs, one_in(m, unreal.MaterialExpressionDDY, stripe_along, -1950, -3480), -1800, -3480), -1650, -3440)
    stripe_far = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, stripe_closeness, -0.125, -1500, -3440), 13.33, -1350, -3440), -1200, -3440)
    fog_picture = picture(m, "Fog", uv, -2600, -1800, numbers=True)
    striped = times(m, times(m, lerp(m, stripe, 0.4, stripe_far, -900, -3350), (occupied, "A"), -750, -3350),
                    times(m, scalar(m, "Stripes", 1.0, -900, -3200), one_in(m, unreal.MaterialExpressionOneMinus, (fog_picture, "R"), -900, -3120), -750, -3160), -600, -3300)
    # The detail, at two sizes so that its repeat does not show. (It is read here, ahead of its use, because the
    # shading of the hills, the edges of the woods and the lie of the fields are roughened by it.)
    fine_uv = world_uv(m, "DetailTiling", 0.0052, -3000, 700)
    coarse_uv = world_uv(m, "DetailTilingCoarse", 0.00137, -3000, 1000)
    fine = picture(m, "Detail", fine_uv, -2300, 700, numbers=True)
    coarse = picture(m, "Detail", coarse_uv, -2300, 1000, numbers=True)
    # (Beyond Europe the heights are known only every two map units, and the edge between a lit slope and a shaded
    # one ran in straight steps a cell long. There the place the shading and the rock are read at is shaken by the
    # fine detail, by about half a cell: "ReliefJitter" is that in the picture's own measure, and nothing in Europe.)
    jitter = vector(m, "ReliefJitter", (0.0, 0.0, 0.0, 0.0), -3300, 150)
    # (The art director's round 32: in West Texas the steps were back, about seven points long. The detail the place
    # was shaken by is a quiet grain round its middle grey: it moved the place by a sixth of a cell and broke
    # nothing. The place is now shaken by a number of its own, made from where the point lies on a grid of thirds of
    # a cell, which runs evenly from nought to one: by a little over half a cell each way. "ReliefJitter" carries
    # the shake in its red and green and that grid in its blue and alpha; all four are nothing in Europe.)
    third = one_in(m, unreal.MaterialExpressionFloor, times(m, uv, pair(m, (jitter, "B"), (jitter, "A"), -3250, 420), -3150, 420), -3050, 420)
    third_x = node_from(m, unreal.MaterialExpressionComponentMask, third, -2950, 400, r=True, g=False, b=False, a=False)
    third_y = node_from(m, unreal.MaterialExpressionComponentMask, third, -2950, 480, r=False, g=True, b=False, a=False)

    def scattered(u, v, plus_by, x, y):
        inner = one_in(m, unreal.MaterialExpressionFrac, plus(m, plus(m, times(m, u, 0.06711056, x, y), times(m, v, 0.00583715, x, y + 60), x + 100, y), plus_by, x + 200, y), x + 300, y)
        return one_in(m, unreal.MaterialExpressionFrac, times(m, inner, 52.9829189, x + 400, y), x + 500, y)
    shake = times(m, plus(m, pair(m, scattered(third_x, third_y, 0.0, -2850, 400), scattered(third_y, third_x, 0.37, -2850, 520), -2250, 460), -0.5, -2150, 460),
                  pair(m, (jitter, "R"), (jitter, "G"), -3150, 150), -2050, 200)
    # (The art director, 19 November 2026: from the nearest eye each third of a cell showed as a square about ten
    # points across, a checker along every edge of light and shade on Idaho's hills: its shading was read from a place
    # of its own. The throw is now blended between a third and its neighbours, eased at the joins, so the place moves
    # smoothly across the ground and no third has an edge. Blended throws keep to the middle more than single ones,
    # so the reach is half as much again: "ReliefShakeReach". "ReliefShakeSmooth" at nought is the old square shake.)
    # (Her ruling the same day: at that reach every edge of shade still showed a fine saw of steps from the usual
    # height, three or four points each: the read was moved by up to three quarters of a cell across a third of
    # one, and the edge folded back on itself. With no shake at all the land looked airbrushed. The reach is a half.)
    shake_place = times(m, uv, pair(m, (jitter, "B"), (jitter, "A"), -3250, 5200), -3150, 5200)
    shake_part = one_in(m, unreal.MaterialExpressionFrac, shake_place, -3050, 5200)
    shake_ease = times(m, times(m, shake_part, shake_part, -2950, 5200), plus(m, times(m, shake_part, -2.0, -2950, 5300), 3.0, -2850, 5300), -2750, 5250)
    ease_x = node_from(m, unreal.MaterialExpressionComponentMask, shake_ease, -2650, 5200, r=True, g=False, b=False, a=False)
    ease_y = node_from(m, unreal.MaterialExpressionComponentMask, shake_ease, -2650, 5300, r=False, g=True, b=False, a=False)

    def thrown(dx, dy, x, y):
        cx = plus(m, third_x, float(dx), x, y) if dx else third_x
        cy = plus(m, third_y, float(dy), x, y + 60) if dy else third_y
        return pair(m, scattered(cx, cy, 0.0, x + 100, y), scattered(cy, cx, 0.37, x + 100, y + 120), x + 700, y + 60)
    throw_near = lerp(m, thrown(0, 0, -2500, 5400), thrown(1, 0, -2500, 5700), ease_x, -1700, 5550)
    throw_far = lerp(m, thrown(0, 1, -2500, 6000), thrown(1, 1, -2500, 6300), ease_x, -1700, 6150)
    throw = lerp(m, throw_near, throw_far, ease_y, -1550, 5850)
    smooth_shake = times(m, times(m, plus(m, throw, -0.5, -1450, 5850), scalar(m, "ReliefShakeReach", 0.5, -1450, 5950), -1350, 5850),
                         pair(m, (jitter, "R"), (jitter, "G"), -1450, 6050), -1250, 5900)
    shake = lerp(m, shake, smooth_shake, scalar(m, "ReliefShakeSmooth", 1.0, -1250, 6000), -1150, 5900)
    relief_uv = plus(m, uv, shake, -2800, 150)
    # (Tried and taken out: shaking the ground's own colour on the world's coarse sheet by the coarser detail, against
    # the straight valleys of New England seen from high up. It combed the coasts and the woods and left the valleys
    # as they were: they are the Hudson, the Green Mountains and the Connecticut, which do run north and south.)
    ground = picture(m, "Ground", uv, -2600, -500)
    relief = picture(m, "Relief", relief_uv, -2600, 0, numbers=True)
    masks = picture(m, "Masks", relief_uv, -2600, 250, numbers=True)
    # (Her trim of round 29: under whole snow a stripe was up to a tenth darker than the snow beside it, and read like
    # a venetian blind. Where the snow lies whole it lies less thin on a stripe, which takes the darkness out; and
    # the stripe's colour is carried further from its holder's, so that it is warmer than its ground, not darker.)
    snow_whole = one_in(m, unreal.MaterialExpressionSaturate, times(m, (masks, "G"), 6.67, -1650, -3100), -1500, -3100)
    # (only where it lies deep: measured, a little of this goes a long way on open snow, and where the snow is thin,
    # at Kiev in January, the stripe needs none of it)
    snow_deep = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, (masks, "G"), -0.095, -1800, -3180), 40.0, -1650, -3180), -1500, -3180)
    stripe_colour = one_in(m, unreal.MaterialExpressionSaturate,
                           plus(m, (tint, "RGB"), times(m, plus(m, (occupied, "RGB"), times(m, (tint, "RGB"), -1.0, -1350, -3050), -1200, -3050),
                                                        plus(m, times(m, snow_deep, scalar(m, "StripesSnowWarm", 0.06, -1350, -2950), -1200, -2950), 1.0, -1050, -2950), -900, -3000), -750, -3050), -600, -3050)
    tint_rgb = lerp(m, (tint, "RGB"), stripe_colour, striped, -450, -3200)

    # The nation's colour is a stain over the bare ground: a wash everywhere (thicker for a nation of little colour,
    # which the Relief picture's alpha says) and a band along the national frontier (the Tint picture's alpha).
    # "Flat" lays the stain on whole, for the map view that shows the countries in flat colour.
    # (The art director's round 31, "blue beside gold": beyond Europe a blue or violet paint mixed into warm ground
    # cancels to grey, and Argentina read as ice. Such a country is marked in the same alpha as the boost, as a half
    # added: its ripe wheat, its trees and its palest ground take no wash, so that the blue lies beside the land's
    # warmth and not in it.)
    cool_paint = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, (relief, "A"), -0.45, -2300, -1000), 40.0, -2200, -1000), -2100, -1000)
    boost = plus(m, (relief, "A"), times(m, cool_paint, -0.5, -2300, -920), -2200, -920)
    wash = times(m, scalar(m, "Wash", 0.34, -2300, -760), plus(m, boost, 1.0, -2300, -700), -2150, -730)
    band = times(m, (tint, "A"), scalar(m, "Band", 0.4, -2300, -640), -2150, -640)
    stain = one_in(m, unreal.MaterialExpressionSaturate, plus(m, wash, band, -2050, -690), -1950, -690)
    stain = lerp(m, stain, 1.0, scalar(m, "Flat", 0.0, -2050, -600), -1850, -660)
    stain = times(m, stain, scalar(m, "Politics", 0.94, -1850, -570), -1750, -630)
    c = lerp(m, (ground, "RGB"), tint_rgb, stain, -2000, -400)
    cover = picture(m, "Cover", uv, -2600, 500, numbers=True)

    # Farmland: a quilt of fields on the low ground. Every field has a tone of its own, from pasture through
    # wheat to plough, read from a picture with one pixel to a field; the rows lean and wander a little so that it
    # is no chessboard. The quilt multiplies whatever is under it, so a nation's fields are still its own colour.
    # Small fields with hedges round them in the west, great open ones on the eastern plains.
    place = world_uv(m, "FieldTiling", 0.03, -3000, 1500)  # (a small field is a strip about three map units long and one wide)
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
    # (The whole-world map: the New World's fields are squares laid on the compass. Where the lie picture says "no
    # turn at all", to the last figure, the rows are not slanted, do not wander, are not strips and are not bonded
    # like brick. No province of Europe is given that turn.)
    square = times(m, one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, (lie, "R"), -0.998, -3300, 2100), 1000.0, -3150, 2100), -3000, 2100),
                   one_in(m, unreal.MaterialExpressionSaturate,
                          plus(m, times(m, one_in(m, unreal.MaterialExpressionAbs, plus(m, (lie, "G"), -0.502, -3300, 2200), -3150, 2200), -400.0, -3000, 2200), 1.0, -2850, 2200), -2700, 2200),
                   -2550, 2150)
    slant = one_in(m, unreal.MaterialExpressionOneMinus, square, -2400, 2150)
    # (a square's size is fixed: its blue says instead how wholly the country is square-field country, see "FieldsFlat")
    size = lerp(m, size, 1.0, square, -2550, 2000)
    east = times(m, plus(m, times(m, east0, cosine, -2600, 1500), times(m, south0, sine, -2600, 1560), -2500, 1500), size, -2400, 1500)
    south = times(m, plus(m, times(m, south0, cosine, -2600, 1640), times(m, times(m, east0, sine, -2600, 1700), -1.0, -2550, 1700), -2500, 1640), size, -2400, 1640)
    rows = pair(m, plus(m, east, times(m, times(m, south, 0.33, -2500, 1560), slant, -2420, 1560), -2350, 1500),
                times(m, plus(m, south, times(m, times(m, east, -0.22, -2500, 1660), slant, -2420, 1660), -2350, 1620), lerp(m, 1.389, 1.0, square, -2300, 1700), -2200, 1620), -2050, 1550)
    rows = plus(m, rows, times(m, times(m, leaning(m, coarse, "R", "G", -2300, 1800), scalar(m, "FieldWander", 0.3, -2000, 1900), -1700, 1800), slant, -1600, 1800), -1500, 1600)
    # Fields are strips, two to four times as long as they are wide, and each row of them is set off from the
    # last like a course of bricks, so that four corners seldom meet and the land does not read as squares.
    row_u = node(m, unreal.MaterialExpressionComponentMask, -1450, 1750, r=True, g=False, b=False, a=False)
    connect(rows, "", row_u, "")
    row_v = times(m, node_from(m, unreal.MaterialExpressionComponentMask, rows, -1450, 1850, r=False, g=True, b=False, a=False),
                  lerp(m, scalar(m, "FieldStrip", 2.2, -1450, 1950), 1.0, square, -1400, 2000), -1350, 1850)
    course = one_in(m, unreal.MaterialExpressionFloor, row_v, -1250, 1950)
    rows = pair(m, plus(m, row_u, times(m, times(m, course, 0.37, -1150, 1950), slant, -1100, 1950), -1050, 1750), row_v, -950, 1800)
    small = picture(m, "Fields", times(m, rows, 1.0 / 64.0, -1300, 1500), -1100, 1500, numbers=True)
    large = picture(m, "Fields", plus(m, times(m, rows, 0.42 / 64.0, -1300, 1700), 0.37, -1150, 1700), -950, 1700, numbers=True)
    tone = times(m, lerp(m, (small, "RGB"), (large, "RGB"), (cover, "G"), -750, 1600), 2.0, -600, 1600)
    # (And the squares are the prairie's wheat, fallow and green, in place of Europe's pasture, wheat and plough.
    # A square is read on a grid of its own: the great open fields' grid where the fields are great, the small
    # fields' where they are small, never a blend of the two. About a third of the squares are split into halves or
    # quarters, each part with a crop of its own; one part in five is green, the rest wheat and fallow.)
    big = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, (cover, "G"), -0.5, -1300, 2500), 1000.0, -1150, 2500), -1000, 2500)
    grid = lerp(m, rows, plus(m, times(m, rows, 0.42, -1300, 2600), 0.37 * 64.0, -1150, 2600), big, -850, 2550)
    square_of = one_in(m, unreal.MaterialExpressionFloor, grid, -700, 2500)
    cell = one_in(m, unreal.MaterialExpressionFrac, grid, -700, 2620)
    # (The art director's ruling of 19 November 2026: "merge 1 to 4 squares into rectangles so it reads as
    # patchwork". The squares are taken in blocks of two by two, and each block throws its own dice: a quarter of the
    # blocks are one field, a quarter two fields side by side, a quarter one above the other, and a quarter four
    # single squares. A joined field reads its crop at the block's own corner, so its squares are one colour.
    # "SquareJoin" at nought lays every square alone, as before.)
    block = times(m, one_in(m, unreal.MaterialExpressionFloor, times(m, square_of, 0.5, -700, 2380), -550, 2380), 2.0, -400, 2380)
    block_dice = picture(m, "Fields", times(m, plus(m, block, 0.5, -250, 2380), 1.0 / 64.0, -100, 2380), 50, 2380, numbers=True)
    join_u = one_in(m, unreal.MaterialExpressionSaturate,
                    times(m, plus(m, one_in(m, unreal.MaterialExpressionFrac, times(m, (block_dice, "R"), 23.1, 200, 2340), 300, 2340), -0.5, 400, 2340), 1000.0, 500, 2340), 600, 2340)
    join_v = one_in(m, unreal.MaterialExpressionSaturate,
                    times(m, plus(m, one_in(m, unreal.MaterialExpressionFrac, times(m, (block_dice, "G"), 31.7, 200, 2420), 300, 2420), -0.5, 400, 2420), 1000.0, 500, 2420), 600, 2420)
    joined = scalar(m, "SquareJoin", 1.0, 600, 2300)
    home = pair(m,
                lerp(m, node_from(m, unreal.MaterialExpressionComponentMask, square_of, 700, 2340, r=True, g=False, b=False, a=False),
                     node_from(m, unreal.MaterialExpressionComponentMask, block, 700, 2380, r=True, g=False, b=False, a=False),
                     times(m, join_u, joined, 750, 2300), 850, 2340),
                lerp(m, node_from(m, unreal.MaterialExpressionComponentMask, square_of, 700, 2420, r=False, g=True, b=False, a=False),
                     node_from(m, unreal.MaterialExpressionComponentMask, block, 700, 2460, r=False, g=True, b=False, a=False),
                     times(m, join_v, joined, 750, 2460), 850, 2420),
                1000, 2380)
    lot = picture(m, "Fields", times(m, plus(m, home, 0.5, -550, 2500), 1.0 / 64.0, -400, 2500), -250, 2500, numbers=True)
    # (which squares are split, and how: a third of them; of those, half in two and half in four)
    dice = one_in(m, unreal.MaterialExpressionFrac, times(m, (lot, "R"), 13.7, -100, 2480), 50, 2480)
    split = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, dice, -1.0, 200, 2480), 0.33, 300, 2480), 1000.0, 400, 2480), 550, 2480)
    dice2 = one_in(m, unreal.MaterialExpressionFrac, times(m, (lot, "G"), 29.3, -100, 2580), 50, 2580)
    in_four = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, dice2, -0.5, 200, 2580), 1000.0, 300, 2580), 450, 2580)
    dice3 = one_in(m, unreal.MaterialExpressionFrac, times(m, (lot, "B"), 17.1, -100, 2680), 50, 2680)
    across = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, dice3, -0.5, 200, 2680), 1000.0, 300, 2680), 450, 2680)
    cell_u = node_from(m, unreal.MaterialExpressionComponentMask, cell, -550, 2760, r=True, g=False, b=False, a=False)
    cell_v = node_from(m, unreal.MaterialExpressionComponentMask, cell, -550, 2840, r=False, g=True, b=False, a=False)
    half_u = one_in(m, unreal.MaterialExpressionFloor, times(m, cell_u, 2.0, -400, 2760), -250, 2760)
    half_v = one_in(m, unreal.MaterialExpressionFloor, times(m, cell_v, 2.0, -400, 2840), -250, 2840)
    cut_u = times(m, split, lerp(m, across, 1.0, in_four, 600, 2700), 750, 2700)
    cut_v = times(m, split, lerp(m, one_in(m, unreal.MaterialExpressionOneMinus, across, 500, 2800), 1.0, in_four, 600, 2800), 750, 2800)
    part_at = pair(m, times(m, times(m, half_u, cut_u, 900, 2700), 19.0, 1000, 2700), times(m, times(m, half_v, cut_v, 900, 2800), 37.0, 1000, 2800), 1150, 2750)
    crop = picture(m, "Fields", times(m, plus(m, plus(m, home, 0.5, 1300, 2700), part_at, 1400, 2750), 1.0 / 64.0, 1550, 2750), 1700, 2750, numbers=True)
    # (which crop: a number from 0 to 1 made of the part's own tone; under 0.2 green, to 0.62 wheat, the rest fallow)
    lots = one_in(m, unreal.MaterialExpressionFrac, plus(m, times(m, (crop, "R"), 7.1, 1900, 2700), times(m, (crop, "G"), 3.3, 1900, 2800), 2050, 2750), 2200, 2750)
    is_green = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, lots, -1.0, 2350, 2700), lerp(m, scalar(m, "SquareGreen", 0.2, 2350, 2800), scalar(m, "CoolSquareGreen", 0.45, 2350, 2850), cool_paint, 2400, 2830), 2450, 2700), 1000.0, 2550, 2700), 2700, 2700)
    is_wheat = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, lots, -1.0, 2350, 2900), lerp(m, scalar(m, "SquareWheat", 0.8, 2350, 2980), scalar(m, "CoolSquareWheat", 0.7, 2350, 3030), cool_paint, 2400, 3010), 2450, 2900), 1000.0, 2550, 2900), 2700, 2900)
    grain = lerp(m, vector(m, "FieldFallow", (0.83, 0.63, 0.83, 1.0), 2700, 3000), vector(m, "FieldWheat", (1.49, 1.28, 1.09, 1.0), 2700, 3100), is_wheat, 2900, 3000)
    grain = lerp(m, grain, vector(m, "FieldGreen", (0.72, 0.91, 0.83, 1.0), 2700, 3200), is_green, 3050, 3050)
    # (each part a little paler or darker than the next of its kind)
    grain = times(m, grain, plus(m, times(m, (crop, "B"), 0.16, 3050, 3200), 0.92, 3150, 3200), 3250, 3100)
    # Furrows inside each part, north to south in some and east to west in others, at about a tenth of the
    # contrast between one square and the next. ("SquareWork" is nothing from high up, where they would shimmer.)
    work = scalar(m, "SquareWork", 1.0, 3250, 3400)
    dice4 = one_in(m, unreal.MaterialExpressionFrac, times(m, (crop, "G"), 11.3, 1900, 3300), 2050, 3300)
    upright = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, dice4, -0.5, 2200, 3300), 1000.0, 2300, 3300), 2450, 3300)
    along = lerp(m, cell_u, cell_v, upright, 2600, 3350)
    ridge = one_in(m, unreal.MaterialExpressionFrac, times(m, along, lerp(m, 7.0, 15.0, big, 2600, 3450), 2750, 3400), 2900, 3400)
    furrow = plus(m, times(m, one_in(m, unreal.MaterialExpressionAbs, plus(m, ridge, -0.5, 3050, 3400), 3150, 3400), 4.0, 3250, 3450), -1.0, 3350, 3450)
    grain = times(m, grain, plus(m, times(m, times(m, furrow, 0.03, 3450, 3450), work, 3550, 3450), 1.0, 3650, 3450), 3750, 3300)
    # (From high up the squares are smaller than a point of the screen and their crops only speckle it: there they
    # are drawn toward the plain's own mean, to about a third of their contrast.)
    grain = lerp(m, lerp(m, vector(m, "FieldMean", (1.5, 1.3, 0.9, 1.0), 3750, 3500), vector(m, "CoolFieldMean", (0.98, 0.98, 0.94, 1.0), 3750, 3560), cool_paint, 3800, 3530), grain,
                 # (the same ruling: "halve the step between neighbouring fields": from near, a field keeps under half of
                 # its own crop's tone over the plain's mean, where it kept all of it)
                 lerp(m, 0.33, scalar(m, "SquareNear", 0.7, 3850, 3560), work, 3850, 3500), 3950, 3400)
    # (The art director's world-detail order, item 2: the squares now end where the survey ended, across open
    # farmland, and their gold stopped there at a ruled line. A square's blue is how wholly the country is
    # square-field country, from nought at its edge to one well inside: at the edge a square's crops are taken back
    # to the ground's own colour (their tones kept, the plain's gold taken out), and the gold comes in with the blue.)
    square_mean = lerp(m, vector(m, "FieldMean", (1.5, 1.3, 0.9, 1.0), 3950, 3620), vector(m, "CoolFieldMean", (0.98, 0.98, 0.94, 1.0), 3950, 3680), cool_paint, 4000, 3650)
    plain_grain = node(m, unreal.MaterialExpressionDivide, 4150, 3550)
    _feed(m, plain_grain, "A", grain)
    _feed(m, plain_grain, "B", square_mean)
    grain = lerp(m, plain_grain, grain, one_in(m, unreal.MaterialExpressionSaturate, times(m, (lie, "B"), scalar(m, "SquareGoldIn", 1.0, 4150, 3700), 4250, 3700), 4350, 3700), 4450, 3550)
    tone = lerp(m, tone, grain, square, -80, 1700)
    # The section roads: a pale line on the squares' own grid, about a point of the screen wide at working height.
    back_u = one_in(m, unreal.MaterialExpressionOneMinus, cell_u, -550, 2920)
    back_v = one_in(m, unreal.MaterialExpressionOneMinus, cell_v, -550, 3000)
    near_u = node(m, unreal.MaterialExpressionMin, -400, 2920)
    connect(cell_u, "", near_u, "A")
    connect(back_u, "", near_u, "B")
    near_v = node(m, unreal.MaterialExpressionMin, -400, 3000)
    connect(cell_v, "", near_v, "A")
    connect(back_v, "", near_v, "B")
    to_road = node(m, unreal.MaterialExpressionMin, -250, 2960)
    connect(near_u, "", to_road, "A")
    connect(near_v, "", to_road, "B")
    road = times(m, times(m, one_in(m, unreal.MaterialExpressionOneMinus,
                                    one_in(m, unreal.MaterialExpressionSaturate, times(m, to_road, lerp(m, 26.0, 62.0, big, -250, 3080), -100, 3000), 50, 3000), 200, 3000),
                             times(m, square, (cover, "R"), 200, 3100), 350, 3050),
                 times(m, work, scalar(m, "SquareRoads", 0.55, 350, 3200), 500, 3200), 650, 3100)
    # (From high up the quilt's pattern is let go but not its colour: "FieldsFlat" draws every field toward the mean
    # of its own manner of field, the squares' gold or the strips' own, so that the plain keeps its colour when the
    # fields are too small to be seen.)
    tone = lerp(m, tone, lerp(m, vector(m, "QuiltMean", (1.0, 1.0, 1.0, 1.0), -400, 1850), lerp(m, vector(m, "FieldMean", (1.5, 1.3, 0.9, 1.0), -400, 1950), vector(m, "CoolFieldMean", (0.98, 0.98, 0.94, 1.0), -400, 2010), cool_paint, -350, 1980),
                              times(m, square, (lie, "B"), -330, 2050), -250, 1900),
                scalar(m, "FieldsFlat", 0.0, -250, 2000), -100, 1850)
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
    # (The same ruling.) The ripe-wheat squares of such a country are laid in wheat's own gold and take none of its
    # paint; from high up, where the squares' pattern is let go, they go back into the country's colour.
    ripe = times(m, times(m, times(m, is_wheat, one_in(m, unreal.MaterialExpressionOneMinus, is_green, -1900, -100), -1800, -100), times(m, square, work, -1800, -40), -1700, -80),
                 times(m, cool_paint, one_in(m, unreal.MaterialExpressionSaturate, times(m, (cover, "R"), 4.0, -1900, 20), -1800, 20), -1700, 0), -1600, -60)
    c = lerp(m, c, lerp(m, (ground, "RGB"), vector(m, "CoolWheat", (0.4, 0.27, 0.075, 1.0), -1900, 100), times(m, stain, scalar(m, "CoolWheatWash", 1.0, -1900, 180), -1800, 140), -1700, 100), ripe, -1500, -200)
    # And where there are no squares (the Patagonian steppe, the Chaco), the palest of the ground, its dry grass and
    # bare rock on the slopes that face the light, keeps its straw: about a fifth to a quarter of the surface stays
    # warm. (First tried by a fine noise, which left white specks that read as snow: it is the land's own light
    # places that stay straw, in patches as broad as the slopes. The ground's picture is read as a colour, so its
    # lightness here is the light's own measure and not the paint's: a dry plain is about a quarter. And the straw
    # is laid a full straw yellow, so that it is never taken for snow on the Andes. Its edge is crisp: brought in
    # softly, the straw and the blue mixed to grey again, which is the very fault. At 0.17 about a quarter of
    # Patagonia is straw; at 0.14 two thirds of the Chaco was, and the country lost its colour.)
    lightness = times(m, (ground, "G"), plus(m, times(m, (relief, "B"), scalar(m, "CoolStrawSun", 0.9, -2000, 420), -1900, 420), 0.55, -1800, 420), -1700, 400)
    straw = times(m, times(m, cool_paint, one_in(m, unreal.MaterialExpressionOneMinus, square, -1900, 260), -1800, 260),
                  one_in(m, unreal.MaterialExpressionSaturate,
                         times(m, plus(m, lightness, times(m, scalar(m, "CoolStraw", 0.17, -2000, 340), -1.0, -1900, 340), -1800, 320), scalar(m, "CoolStrawSharp", 30.0, -1900, 500), -1700, 340), -1600, 340), -1500, 300)
    straw_colour = lerp(m, (ground, "RGB"), vector(m, "CoolStrawColour", (0.46, 0.35, 0.15, 1.0), -1700, 520), scalar(m, "CoolStrawTint", 0.85, -1700, 600), -1600, 560)
    # (The art director's round 33, ruling B: "the quilt gone wild". A cool paint laid flat over open ground reads as
    # a lake, and patches shaped like the ground read as sandbanks in a sea. What reads as land is what the pampas
    # already has: crisp patches side by side. So flat wild ground under such a paint is laid in paddocks, about three
    # farm squares across, with crooked sides: the country's colour in three tones, about a quarter of them left
    # straw and about one in ten left as scrub, no blend anywhere. On a slope the lightness rule above stands: lit
    # sides in the land's own warm rock, shade in the country's colour.)
    rg = plus(m, times(m, pair(m, (relief, "R"), (relief, "G"), -2300, 700), 2.0, -2200, 700), -1.0, -2100, 700)
    steepness = node(m, unreal.MaterialExpressionDotProduct, -2000, 700)
    connect(rg, "", steepness, "A")
    connect(rg, "", steepness, "B")
    flat_here = one_in(m, unreal.MaterialExpressionOneMinus, one_in(m, unreal.MaterialExpressionSaturate,
                       times(m, plus(m, steepness, times(m, scalar(m, "WildFlat", 0.006, -2000, 780), -1.0, -1900, 780), -1800, 740), 400.0, -1700, 740), -1600, 740), -1500, 740)
    # (and only open ground: under a canopy the straw paddocks showed through the leaves as pale stars)
    open_ground = one_in(m, unreal.MaterialExpressionOneMinus, one_in(m, unreal.MaterialExpressionSaturate, times(m, (ground, "A"), 3.0, -2000, 920), -1900, 920), -1800, 920)
    wild = times(m, times(m, times(m, cool_paint, one_in(m, unreal.MaterialExpressionOneMinus, square, -1900, 860), -1800, 860), flat_here, -1400, 800), open_ground, -1350, 840)
    wild_scale = scalar(m, "WildPaddock", 0.5, -2400, 940)
    wild_u = times(m, east0, wild_scale, -2300, 900)
    wild_v = times(m, south0, wild_scale, -2300, 980)

    def wobble(a, b, f1, p1, f2, p2, x, y):
        s1 = one_in(m, unreal.MaterialExpressionSine, plus(m, times(m, b, f1, x, y), p1, x + 100, y), x + 200, y)
        s2 = one_in(m, unreal.MaterialExpressionSine, plus(m, times(m, b, f2, x, y + 60), p2, x + 100, y + 60), x + 200, y + 60)
        return plus(m, a, plus(m, times(m, s1, 0.2, x + 300, y), times(m, s2, 0.1, x + 300, y + 60), x + 400, y), x + 500, y)
    # (the engine's sine turns once for each whole number, so these are turns a paddock: a slow crook in each side
    # and a lesser one on it. Asked in radians at first, every side came out with five teeth, a field of stars.)
    wild_x = wobble(wild_u, wild_v, 0.33, 0.21, 0.84, 0.06, -2200, 1060)
    wild_y = wobble(wild_v, wild_u, 0.30, 0.11, 0.75, 0.35, -2200, 1200)
    # (its own corner of the picture of the fields' tones, read at the middle of a paddock)
    wild_cell = pair(m, plus(m, one_in(m, unreal.MaterialExpressionFloor, wild_x, -1600, 1060), 17.5, -1500, 1060),
                     plus(m, one_in(m, unreal.MaterialExpressionFloor, wild_y, -1600, 1200), 41.5, -1500, 1200), -1400, 1120)
    wild_lot = picture(m, "Fields", times(m, wild_cell, 1.0 / 64.0, -1300, 1160), -1150, 1160, numbers=True)
    wild_dice = one_in(m, unreal.MaterialExpressionFrac, plus(m, times(m, (wild_lot, "R"), 7.1, -1000, 1120), times(m, (wild_lot, "G"), 3.3, -1000, 1200), -900, 1160), -800, 1160)

    def under(value, edge, x, y):
        return one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, times(m, value, -1.0, x, y), edge, x + 100, y), 1000.0, x + 200, y), x + 300, y)
    wild_straw = under(wild_dice, scalar(m, "WildStraw", 0.25, -700, 1060), -700, 1100)
    wild_scrub = times(m, under(wild_dice, 0.36, -700, 1200), one_in(m, unreal.MaterialExpressionOneMinus, wild_straw, -400, 1240), -300, 1200)
    # (three tones of the country's colour: a paddock a fifth darker, one as it is, one a fifth lighter)
    wild_tone = plus(m, 0.8, plus(m, times(m, one_in(m, unreal.MaterialExpressionOneMinus, under(wild_dice, 0.58, -700, 1320), -400, 1320), 0.2, -300, 1320),
                                  times(m, one_in(m, unreal.MaterialExpressionOneMinus, under(wild_dice, 0.8, -700, 1420), -400, 1420), 0.2, -300, 1420), -200, 1370), -100, 1340)
    wild_washed = times(m, lerp(m, (ground, "RGB"), tint_rgb, scalar(m, "WildWash", 0.76, -700, 1520), -600, 1520), wild_tone, -400, 1520)
    wild_colour = lerp(m, wild_washed, straw_colour, wild_straw, -200, 1500)
    wild_colour = lerp(m, wild_colour, times(m, (ground, "RGB"), vector(m, "WildScrub", (0.5, 0.62, 0.4, 1.0), -400, 1620), -300, 1620), wild_scrub, -100, 1560)
    # (from high up the paddocks are too small to see: there the ground goes back to the country's colour whole)
    wild = times(m, wild, work, -1300, 860)
    c = lerp(m, c, straw_colour, times(m, straw, one_in(m, unreal.MaterialExpressionOneMinus, wild, -1400, -100), -1350, -130), -1300, -150)
    c = lerp(m, c, wild_colour, wild, -1200, -150)
    c = times(m, times(m, c, quilt, -1750, -330), hedged, -1650, -330)
    c = lerp(m, c, vector(m, "SquareRoadColour", (0.552, 0.479, 0.262, 1.0), -1750, -200), road, -1550, -330)
    # (The art director's review of build 1608: beyond Europe the crooked fields read as brushed cloth. There they are
    # whole plots on the great open fields' grid, about thirty-seven dots by twelve from playing height, and each has
    # a thin light edge: the plot's own colour, lifted. "PlotEdge" is nothing on Europe's own sheet, whose small
    # fields keep their hedges; the game sets it on the sheets beyond.)
    plot_grid = plus(m, times(m, rows, 0.42, -1300, 3300), 0.37 * 64.0, -1150, 3300)
    plot_part = one_in(m, unreal.MaterialExpressionFrac, plot_grid, -1000, 3300)
    plot_back = one_in(m, unreal.MaterialExpressionOneMinus, plot_part, -850, 3380)
    plot_near = node(m, unreal.MaterialExpressionMin, -700, 3300)
    connect(plot_part, "", plot_near, "A")
    connect(plot_back, "", plot_near, "B")
    plot_u = node_from(m, unreal.MaterialExpressionComponentMask, plot_near, -550, 3300, r=True, g=False, b=False, a=False)
    plot_v = node_from(m, unreal.MaterialExpressionComponentMask, plot_near, -550, 3380, r=False, g=True, b=False, a=False)
    # (a plot is about three times as long as it is wide: the edge is as thin across the narrow way as along the long)
    plot_to = node(m, unreal.MaterialExpressionMin, -400, 3340)
    connect(times(m, plot_u, scalar(m, "PlotEdgeFine", 62.0, -550, 3460), -450, 3300), "", plot_to, "A")
    connect(times(m, plot_v, times(m, scalar(m, "PlotEdgeFine", 62.0, -550, 3520), 1.0 / (2.2 * 1.389), -450, 3520), -350, 3420), "", plot_to, "B")
    plot_line = one_in(m, unreal.MaterialExpressionOneMinus, one_in(m, unreal.MaterialExpressionSaturate, plot_to, -250, 3340), -100, 3340)
    plot_edge = times(m, times(m, plot_line, scalar(m, "PlotEdge", 0.0, -100, 3440), 50, 3380),
                      times(m, times(m, (cover, "R"), big, 50, 3500), slant, 200, 3480), 350, 3420)
    c = times(m, c, plus(m, plot_edge, 1.0, 500, 3420), -1400, -330)

    # Woods are one dark cloth laid over the paint: a standing mass with a ragged edge, thick enough to read as
    # woods on any nation's colour, fir-dark in the north and on the high ground. The woods take the nation's
    # stain too, at about half its strength, so that a nation's wood is still seen to be its own.
    # (its edge is broken at two scales, a fine one and a broad one: the broad one cuts clearings into a big wood
    # and throws a copse or two out beyond its edge; then the edge is made firm, a shape and not a fade)
    ragged = plus(m, times(m, plus(m, (fine, "G"), -0.5, -2500, -1250), scalar(m, "WoodsRag", 0.5, -2500, -1150), -2350, -1250),
                  times(m, plus(m, (coarse, "G"), -0.5, -2500, -1400), scalar(m, "WoodsRagBroad", 1.1, -2500, -1320), -2350, -1400), -2250, -1320)
    def larger(a, b, x=0, y=0):
        n = node(m, unreal.MaterialExpressionMax, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    def over(a, b, x=0, y=0):
        n = node(m, unreal.MaterialExpressionDivide, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    def sat(v, x=0, y=0):
        return one_in(m, unreal.MaterialExpressionSaturate, v, x, y)

    def less(a, b, x=0, y=0):
        return plus(m, a, times(m, b, -1.0, x - 100, y), x, y)

    wet = picture(m, "Wet", uv, -2600, 2600, numbers=True)
    wet_ink = picture(m, "WetInk", uv, -2600, 2850)
    wet_reach = scalar(m, "WetReach", 4.0, -2600, 3080)
    wet_cell = scalar(m, "WetCell", 1.5, -2600, 3160)
    # (which way the water's own line lies and how far, in map units: read between the picture's points it comes to
    # nothing exactly on the line)
    to_line = times(m, times(m, plus(m, pair(m, (wet, "R"), (wet, "G"), -2450, 2600), -0.5, -2350, 2600), 2.0, -2250, 2600), wet_reach, -2150, 2600)
    squared_off = node(m, unreal.MaterialExpressionDotProduct, -2050, 2600)
    connect(to_line, "", squared_off, "A")
    connect(to_line, "", squared_off, "B")
    off = one_in(m, unreal.MaterialExpressionSquareRoot, squared_off, -1950, 2600)
    apart = times(m, (wet, "B"), wet_reach, -2350, 2700)
    half = times(m, (wet, "A"), 2.5, -2350, 2780)
    # (how many map units one pixel of the screen covers there)
    px = larger(plus(m, one_in(m, unreal.MaterialExpressionAbs, one_in(m, unreal.MaterialExpressionDDX, off, -1850, 2700), -1750, 2700),
                     one_in(m, unreal.MaterialExpressionAbs, one_in(m, unreal.MaterialExpressionDDY, off, -1850, 2780), -1750, 2780), -1650, 2740), 0.002, -1550, 2740)
    # (The art director's ruling of round 25: three ranks. A great river is never lost: from the top of the zoom it is
    # one even line two or three pixels across. A main tributary is gone by then, and a hairline sooner, by the time
    # the eye is a little above its working height: or from high up the land is a web of veins.)
    great = sat(times(m, plus(m, half, -0.8, -2150, 2880), 8.0, -2050, 2880), -1950, 2880)
    far = sat(times(m, plus(m, px, -0.3, -1700, 2960), 5.0, -1600, 2960), -1500, 2960)
    half_seen = lerp(m, half, larger(times(m, px, plus(m, scalar(m, "WetLeast", 1.25, -1700, 3040),
                                                                  # (The art director's round 31: from the top the Amazon should be the one river
                                                                  # to be picked out across its basin. Every great river was drawn there at the
                                                                  # one least width. The very greatest, half-broad beyond a map unit, are drawn
                                                                  # up to as broad again and a little more.)
                                                                  times(m, sat(times(m, plus(m, half, -1.0, -1900, 3120), 2.0, -1800, 3120), -1700, 3120), scalar(m, "WetGreatest", 2.5, -1700, 3180), -1600, 3140),
                                                                  -1600, 3060), -1550, 3040), times(m, half, 0.5, -1550, 3100), -1400, 3060), times(m, great, far, -1400, 2960), -1250, 2900)
    inside = less(sat(plus(m, over(less(half_seen, off, -1100, 2600), px, -950, 2600), 0.5, -800, 2600), -650, 2600),
                  sat(plus(m, over(less(times(m, half_seen, -1.0, -1100, 2700), off, -950, 2700), px, -800, 2700), 0.5, -650, 2700), -500, 2700), -350, 2650)
    # (the picture's points round about must all look to this one line: where they look to different waters, the
    # distance with no direction to it is far more than the one read with it, and no line is there)
    truly = sat(times(m, over(less(times(m, wet_cell, scalar(m, "WetTrue", 0.62, -1250, 2880), -1100, 2800), less(apart, off, -950, 2850), -800, 2800), wet_cell, -650, 2800), 4.0, -500, 2800), -350, 2800)
    # (deep inside a broad water, a lake above all, the distance with no direction to it says so by itself: along the
    # middle of a lake made of two stretches at a bend the points look to both, and a pale thread showed down it)
    truly = larger(truly, sat(over(less(half, apart, -950, 2900), px, -800, 2900), -650, 2900), -500, 2900)
    goes_at = lerp(m, scalar(m, "WetHairGoes", 0.19, -1100, 2950), scalar(m, "WetTributaryGoes", 0.36, -1100, 3010), sat(times(m, plus(m, half, -0.16, -950, 2950), 3.3, -850, 2950), -750, 2950), -650, 2950)
    seen_from_here = lerp(m, one_in(m, unreal.MaterialExpressionOneMinus, sat(over(less(px, goes_at, -500, 2950), times(m, goes_at, 0.4, -500, 3010), -400, 2950), -300, 2950), -200, 2950), 1.0, great, -100, 2950)

    def smaller(a, b, x=0, y=0):
        n = node(m, unreal.MaterialExpressionMin, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    # (The art director's ruling of round 27: from higher, red clearings are kept only beside the great river. A
    # clearing beside a lesser river closes over as that river goes from sight: the red earth is known by its colour
    # in the ground's picture, in the jungle and near a water, and the canopy is laid back over it by as much as
    # the eye has risen above its working height (a little before that river itself goes from sight: the art director
    # found the clearings beading the tributaries from higher, wider than the water they sat on). At working height,
    # where the river shows, so does its clearing.)
    # (red earth is more than half as red again as it is green: no other ground under the canopy is)
    red_earth = sat(times(m, less((ground, "R"), times(m, (ground, "G"), 1.6, -3400, -1500), -3200, -1500), 40.0, -3100, -1500), -3000, -1500)
    in_jungle = sat(smaller(times(m, one_in(m, unreal.MaterialExpressionOneMinus, (lie, "A"), -3300, -1420), 2.0, -3200, -1420),
                            times(m, plus(m, (lie, "A"), -0.5, -3300, -1340), 40.0, -3200, -1340), -3100, -1380), -3000, -1380)
    by_a_water = times(m, sat(times(m, half, 400.0, -3300, -1260), -3200, -1260), sat(less(plus(m, half, 4.0, -3300, -1180), off, -3200, -1180), -3100, -1180), -3000, -1220)
    closed_over = times(m, times(m, red_earth, in_jungle, -2900, -1440), times(m, by_a_water, times(m, one_in(m, unreal.MaterialExpressionOneMinus, great, -3000, -1100),
                                                                                       sat(times(m, less(px, scalar(m, "ClearingsGoFrom", 0.235, -3100, -1040), -3000, -1040), 20.0, -2950, -1040), -2900, -1040), -2850, -1080), -2900, -1160), -2800, -1300)
    wood_share = larger((ground, "A"), times(m, closed_over, scalar(m, "ClearingsClose", 0.97, -2800, -1180), -2700, -1240), -2600, -1240)
    # (Beyond Europe the painter lays the edge of a wood as a slow thinning-out, and "WoodsFringe" lets each
    # tree-top stand or fall by itself across it: most where the wood is half there, not at all where it is whole
    # or where there is none. It is nothing on Europe's own sheet, whose woods keep their firm edge.)
    thin = times(m, times(m, wood_share, one_in(m, unreal.MaterialExpressionOneMinus, wood_share, -2700, -1050), -2600, -1050), 4.0, -2500, -1050)
    # (How tall the tree-top at a point stands, 0 to 1, read back out of the detail's picture: across a thin edge
    # only the tallest crowns stand at first, as single trees, then more of them, until they close up.)
    crown_high = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, (fine, "G"), -0.2, -2850, -980), 2.94, -2750, -980), -2700, -980)
    ragged = plus(m, ragged, times(m, times(m, plus(m, crown_high, -0.5, -2650, -980), scalar(m, "WoodsFringe", 0.0, -2700, -900), -2600, -950), thin, -2450, -1000), -2300, -1100)
    # ("WoodsLift" asks more of a wood before it is drawn: from high up, on the world's coarse sheet, the smallest
    # patches are let go, where they were so many same-sized spots.)
    wooded = one_in(m, unreal.MaterialExpressionSaturate,
                    plus(m, times(m, plus(m, plus(m, wood_share, ragged, -2200, -1250),
                                          plus(m, times(m, scalar(m, "WoodsLift", 0.0, -2200, -1150), -1.0, -2120, -1150), -0.5, -2080, -1200), -2050, -1250),
                               scalar(m, "WoodsSharp", 6.0, -2050, -1150), -1900, -1250), 0.5, -1750, -1250), -1600, -1250)
    # (a warm stain, red or orange, turns dark green to brown-black: the woods of such a nation take less of it)
    tint_r = node(m, unreal.MaterialExpressionComponentMask, -2500, -1550, r=True, g=False, b=False, a=False)
    connect(tint, "RGB", tint_r, "")
    tint_g = node(m, unreal.MaterialExpressionComponentMask, -2500, -1480, r=False, g=True, b=False, a=False)
    connect(tint, "RGB", tint_g, "")
    warm = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, tint_r, times(m, tint_g, -1.0, -2350, -1480), -2250, -1520), 2.5, -2150, -1520), -2050, -1520)
    cool = plus(m, times(m, warm, scalar(m, "WoodsWarmLess", -0.45, -2050, -1440), -1950, -1500), 1.0, -1850, -1500)
    leaf = lerp(m, vector(m, "WoodsColour", (0.05, 0.09, 0.04, 1.0), -2000, -900),
                vector(m, "FirColour", (0.024, 0.056, 0.04, 1.0), -2000, -1050), (cover, "A"), -1800, -950)
    # (The whole-world map: the canopy of the tropics is a lighter, fuller green than Europe's woods. How much of the
    # canopy at a place is Europe's is in the alpha of the picture of the fields' lie: 1 everywhere in Europe.)
    # (One number carries two shares: from 1 down to a half, how far the canopy is the tropics'; from a half down to
    # nought, how far it is the northern forest's, a lifted and cooler green than a fir wood of the Alps.)
    not_a = one_in(m, unreal.MaterialExpressionOneMinus, (lie, "A"), -2250, -1250)
    is_north = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, not_a, -0.5, -2100, -1300), 400.0, -1950, -1300), -1800, -1300)
    tropic = times(m, one_in(m, unreal.MaterialExpressionSaturate, times(m, not_a, 2.04, -2100, -1220), -1950, -1220),
                   one_in(m, unreal.MaterialExpressionOneMinus, is_north, -1650, -1300), -1500, -1250)
    north = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, not_a, -0.51, -2100, -1380), 2.04, -1950, -1380), -1800, -1380)
    leaf = lerp(m, leaf, vector(m, "TropicColour", (0.05, 0.108, 0.074, 1.0), -2000, -1200), tropic, -1350, -1100)
    # (The northern share runs in two legs. Up to four tenths the canopy goes from Europe's woods to the broadleaf
    # woods of the temperate lands beyond Europe, darker and bluer than Europe's; from there on, to the northern
    # forest's. The painter gives temperate country four tenths and the northern forest the whole.)
    leaf = lerp(m, leaf, vector(m, "FarWoodsColour", (0.025, 0.047, 0.04, 1.0), -2000, -1600),
                one_in(m, unreal.MaterialExpressionSaturate, times(m, north, 2.5, -1650, -1380), -1500, -1380), -1280, -1100)
    leaf = lerp(m, leaf, vector(m, "NorthColour", (0.045, 0.085, 0.07, 1.0), -2000, -1450),
                one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, north, -0.4, -1650, -1460), 1.6667, -1500, -1460), -1350, -1460), -1200, -1100)
    # (on a warm stain the canopy itself is olive: a very dark green goes chocolate with the least red showing through)
    leaf = times(m, leaf, lerp(m, 1.0, vector(m, "WoodsWarmTint", (1.0, 1.0, 1.0, 1.0), -1800, -1180), warm, -1700, -1100), -1600, -1000)
    # (The art director's round 33, ruling A: a stain keeps the canopy's grain and takes the country's hue. A purple
    # or a magenta mixed part-way into green cancels to grey, which is what Peru's forest was: (88, 86, 83). The
    # canopy of the tropics takes such a paint nearly whole and a little deeper, so that it comes out the country's
    # own colour: Peru's at about (104, 70, 112).)
    tint_b = node(m, unreal.MaterialExpressionComponentMask, -2500, -1410, r=False, g=False, b=True, a=False)
    connect(tint, "RGB", tint_b, "")
    # (Which paints: those with as much blue as red and little green, Peru's violet above all. Not a red or a crimson,
    # which has red far above its blue: Britain's and Japan's jungles keep the olive canopy of a warm stain.)
    purple = times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate,
                                      times(m, plus(m, plus(m, smaller(tint_r, tint_b, -2400, -1700), times(m, tint_g, -1.0, -2400, -1640), -2300, -1680), -0.07, -2200, -1680),
                                            scalar(m, "WoodsPurple", 40.0, -2200, -1600), -2100, -1660), -2000, -1660),
                             one_in(m, unreal.MaterialExpressionSaturate,
                                    plus(m, times(m, plus(m, tint_b, times(m, tint_r, -0.62, -2400, -1780), -2300, -1780), 12.0, -2200, -1780), 0.2, -2100, -1780), -2000, -1780), -1950, -1720), tropic, -1900, -1660)
    woods = lerp(m, leaf,
                 times(m, tint_rgb, lerp(m, scalar(m, "WoodsStainDepth", 0.5, -2300, -1000), scalar(m, "WoodsPurpleDepth", 1.0, -2300, -940), purple, -2200, -970), -2100, -1000),
                 # (round 31: the trees of a blue or violet country beyond Europe take no wash, or its creek lines read as channels of water)
                 times(m, times(m, times(m, stain, scalar(m, "WoodsStain", 0.5, -2300, -1080), -2100, -1080), cool, -1950, -1080),
                       # (but the canopy of the tropics still takes it: a jungle country that took none, as Colombia, lost its
                       # colour altogether, and every tile is a shade of its holder's)
                       one_in(m, unreal.MaterialExpressionOneMinus, times(m, cool_paint, one_in(m, unreal.MaterialExpressionOneMinus, tropic, -2050, -1180), -1950, -1140), -1850, -1140), -1850, -1100), -1650, -950)
    woods = lerp(m, woods, times(m, tint_rgb, scalar(m, "WoodsPurpleDepth", 1.0, -1700, -880), -1600, -880), times(m, purple, scalar(m, "WoodsPurpleShare", 0.84, -1700, -820), -1600, -820), -1500, -900)
    c = lerp(m, c, woods,
             one_in(m, unreal.MaterialExpressionSaturate,
                    times(m, times(m, wooded, scalar(m, "WoodsStrength", 0.84, -2300, -900), -2000, -700),
                          plus(m, times(m, warm, scalar(m, "WoodsWarmMore", 0.0, -2000, -620), -1900, -640), 1.0, -1800, -640), -1700, -680), -1600, -680), -1500, -500)
    # The rim of a wood: lighter on its sunward edge, and a strip of shade on the open ground in its lee.
    c = times(m, c, plus(m, 1.0, times(m, plus(m, (cover, "B"), -0.5, -1500, -700), scalar(m, "WoodsRim", 0.6, -1500, -600), -1350, -650), -1200, -650), -1100, -500)
    c = lerp(m, c, vector(m, "RockColour", (0.235, 0.205, 0.162, 1.0), -2000, -150), (masks, "R"), -1000, -400)

    # The waters of the world beyond Europe (the whole-world map): its rivers and its small lakes, laid over the
    # ground and whatever stain is on it, since water is nobody's colour. Europe's own rivers are ribbons laid by
    # the rules, and its picture of the waters says "none". (CampaignGround.h says what the two pictures hold.)
    # A river is drawn from how far each point stands from its line, which the picture knows to one side and the
    # other: read between the picture's points, that distance crosses nothing exactly on the line, so a river far
    # narrower than the picture's own points still has a clean bank at any height of the eye.
    # (A lake is a broad water made of two or three stretches. Round its edge the picture's points look to one stretch
    # or another, and where they disagreed the edge was bitten into: nicks, which the art director ruled out. A broad
    # water with a steady shore, which is a lake, is drawn from the distance alone, which has no direction to
    # disagree about and rounds every corner a little. Rivers, and every hairline, are drawn as before.)
    broad_shape = less(sat(plus(m, over(less(half_seen, apart, -1100, 3200), px, -950, 3200), 0.5, -800, 3200), -650, 3200), 0.0, -500, 3200)
    steady_water = sat(times(m, plus(m, (wet_ink, "A"), -0.495, -800, 3280), 100.0, -650, 3280), -500, 3280)
    broadly = times(m, sat(over(less(half, times(m, wet_cell, 0.7, -1100, 3360), -950, 3360), times(m, wet_cell, 0.5, -950, 3420), -800, 3360), -650, 3360), steady_water, -500, 3360)
    water_shape = lerp(m, times(m, inside, truly, -200, 2700), broad_shape, broadly, -150, 2720)
    river = times(m, times(m, water_shape, seen_from_here, -100, 2750), sat(times(m, half, 400.0, -350, 3020), -200, 3020), -50, 2750)
    # (its bed: dry sand from the banks inward, by the share the picture gives. Under six tenths, the bed comes and
    # goes along the river, as bars do.)
    bars_place = world_uv(m, "BarsTiling", 0.0045, -3000, 3300)
    bars_east = node_from(m, unreal.MaterialExpressionComponentMask, bars_place, -2300, 3300, r=True, g=False, b=False, a=False)
    bars_south = node_from(m, unreal.MaterialExpressionComponentMask, bars_place, -2300, 3380, r=False, g=True, b=False, a=False)
    swing = one_in(m, unreal.MaterialExpressionSine, plus(m, times(m, bars_south, 6.2832, -2150, 3380), times(m, one_in(m, unreal.MaterialExpressionSine, times(m, bars_east, 4.3, -2150, 3300), -2000, 3300), 1.9, -1850, 3300), -1700, 3340), -1550, 3340)
    swing2 = one_in(m, unreal.MaterialExpressionSine, plus(m, times(m, bars_east, 7.9, -2150, 3460), times(m, bars_south, 2.3, -2150, 3540), -2000, 3500), -1850, 3500)
    bars = sat(plus(m, times(m, plus(m, swing, swing2, -1400, 3420), 2.0, -1250, 3420), -2.1, -1100, 3420), -950, 3420)  # (a bar here and there, not a kerb)
    steady = sat(times(m, plus(m, (wet_ink, "A"), -0.495, -800, 3100), 100.0, -650, 3100), -500, 3100)
    share = lerp(m, times(m, times(m, (wet_ink, "A"), 2.04, -800, 3000), bars, -650, 3000), sat(times(m, plus(m, (wet_ink, "A"), -0.5, -800, 3160), 2.0, -650, 3160), -500, 3160), steady, -350, 3050)
    bed = sat(plus(m, over(less(off, times(m, half_seen, one_in(m, unreal.MaterialExpressionOneMinus, share, -350, 3150), -200, 3150), -50, 3100), px, 100, 3100), 0.5, 250, 3100), 400, 3100)
    water_colour = lerp(m, (wet_ink, "RGB"), vector(m, "BedColour", (0.36, 0.334, 0.2, 1.0), -200, 3250), bed, 550, 3050)
    # (The art director's round 30: by night the creeks were an electric cobalt, (3, 40, 112), darker than the ground
    # and far bluer. A lesser painted water sinks six tenths of the way toward the ground it runs over by moonlight
    # (at a half its thread still measured (13, 47, 101); the asking was about (25, 56, 104)). The great rivers keep
    # their pale ribbon, which is the accepted look.)
    lesser = one_in(m, unreal.MaterialExpressionOneMinus, sat(times(m, plus(m, half, -0.3, 300, 3400), 2.5, 400, 3400), 500, 3400), 600, 3400)
    water_colour = lerp(m, water_colour, c, times(m, times(m, scalar(m, "WetMoon", 0.0, 300, 3500), scalar(m, "WetNightGround", 0.6, 300, 3580), 450, 3540), lesser, 600, 3500), 700, 3150)
    # THE MOVING WATERS of the land's own paint (the moving-water brief, section 5). A lake takes the sea's recipe at
    # about 0.55 of the strength and 0.75 of the size: one swell and one chop from the game's picture of the waves,
    # sliding downwind. A painted river takes only a faint slide of the same. ("WatersMove" is the game's: half by
    # night, nothing from high up, nothing on the paper.)
    import math
    wet_place = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionWorldPosition, -3600, 3700), -3400, 3700, r=True, g=True, b=False, a=False)
    wet_wind = math.atan2(0.9, 0.25)

    def wet_slide(angle, length, speed, x, y):
        a = wet_wind + math.radians(angle)
        u = node(m, unreal.MaterialExpressionDotProduct, x, y)
        connect(wet_place, "", u, "A")
        connect(node(m, unreal.MaterialExpressionConstant2Vector, x - 220, y + 80, r=math.cos(a) / length, g=math.sin(a) / length), "", u, "B")
        v = node(m, unreal.MaterialExpressionDotProduct, x, y + 180)
        connect(wet_place, "", v, "A")
        connect(node(m, unreal.MaterialExpressionConstant2Vector, x - 220, y + 260, r=-math.sin(a) / length, g=math.cos(a) / length), "", v, "B")
        going = node(m, unreal.MaterialExpressionPanner, x + 440, y, speed_x=-speed / length, speed_y=0.0)
        connect(pair(m, u, v, x + 220, y), "", going, "Coordinate")
        return going

    wet_wave = plus(m, plus(m, times(m, (picture(m, "Waves", wet_slide(0.0, 198.0, 7.5, -3000, 3700), -2200, 3700, numbers=True), "R"), 1.3, -1900, 3700),
                            times(m, (picture(m, "Waves", wet_slide(25.0, 144.0, 3.75, -3000, 4150), -2200, 4150, numbers=True), "G"), 0.55, -1900, 4150), -1700, 3900), -0.925, -1550, 3900)
    wet_heave = plus(m, times(m, one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, wet_wave, 1.7, -1400, 3850), -0.3, -1250, 3850), -1100, 3850), 0.34, -950, 3850),
                     times(m, one_in(m, unreal.MaterialExpressionSaturate, times(m, wet_wave, -1.2, -1400, 4000), -1100, 4000), -0.13, -950, 4000), -800, 3900)
    wet_moved = times(m, times(m, wet_heave, lerp(m, 0.2, 0.55, broadly, -800, 4100), -650, 3950), scalar(m, "WatersMove", 1.0, -800, 4250), -500, 3950)
    water_colour = times(m, water_colour, plus(m, wet_moved, 1.0, -350, 3950), 850, 3200)
    c = lerp(m, c, water_colour, river, -950, -400)
    waters = river
    wetness = times(m, river, one_in(m, unreal.MaterialExpressionOneMinus, bed, 550, 3250), 700, 3250)
    # (The art director's round 28: under snow the stripes of occupied ground went quiet, and in the snowed woods of
    # the East they could not be seen at all, where what could be liberated matters most. The snow lies thinner on a
    # stripe: the old owner's colour shows through the dusting, as a dark thread shows through frost on cloth.)
    # (Her trim of round 29: under full snow a stripe was up to a tenth darker than the snow beside it. Where the snow
    # lies full it lies less thin on a stripe, so that the stripe there is warmer than its ground and hardly darker.)
    thin_snow = lerp(m, scalar(m, "StripesThinSnow", 0.38, -1500, -40), scalar(m, "StripesThinSnowFull", 0.28, -1500, 100), snow_whole, -1350, 20)
    snow_lying = times(m, (masks, "G"), one_in(m, unreal.MaterialExpressionOneMinus, times(m, striped, thin_snow, -1350, -60), -1200, -60), -1050, -100)
    c = lerp(m, c, vector(m, "SnowColour", (0.888, 0.871, 0.823, 1.0), -1700, -150), snow_lying, -900, -400)
    # (The art director's ruling of round 19: in winter the snow took a wood's dark floor away, and every tree stood
    # alone on white like salt. A wood keeps a cool grey floor under the snow, like pencil shading on the map, about
    # a tenth below the snow round it; and so the woods can be found in winter, which matters, since they change a fight.)
    c = lerp(m, c, times(m, c, vector(m, "SnowWoodFloor", (0.62, 0.66, 0.71, 1.0), -1700, -60), -1300, -330),
             times(m, wooded, (masks, "G"), -1300, -260), -850, -330)
    c = lerp(m, c, vector(m, "SandColour", (0.694, 0.597, 0.371, 1.0), -1400, -150), (masks, "B"), -800, -400)

    # Which kind of detail shows where: grass by default, tree-tops where it is wooded, rock where rock shows,
    # sand on the shore and in the desert, nothing on snow.
    sandy = one_in(m, unreal.MaterialExpressionSaturate, plus(m, (masks, "B"), (masks, "A"), -2300, 500), -2100, 500)

    # (On a great roof of canopy the one picture of tree-tops, repeating, shows as a lattice. Near to, the tree-tops
    # are read a second time, a third finer and turned a quarter round, and a crown is wherever either has one.)
    fine_u = node_from(m, unreal.MaterialExpressionComponentMask, fine_uv, -3000, 850, r=True, g=False, b=False, a=False)
    fine_v = node_from(m, unreal.MaterialExpressionComponentMask, fine_uv, -3000, 930, r=False, g=True, b=False, a=False)
    turned_uv = pair(m, times(m, fine_v, 1.37, -2850, 930), times(m, fine_u, -1.37, -2850, 850), -2700, 890)
    turned = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -2500, 890, parameter_name="Detail", texture=stand_in(True),
                  sampler_type=sampler_for(stand_in(True)), sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    connect(turned_uv, "", turned, "UVs")
    crowns = node(m, unreal.MaterialExpressionMax, -2250, 890)
    connect(fine, "G", crowns, "A")
    connect(turned, "G", crowns, "B")

    def kind_of_ground(sample, x, y, trees=None):
        v = lerp(m, (sample, "R"), trees if trees is not None else (sample, "G"), wooded, x, y)
        v = lerp(m, v, (sample, "B"), (masks, "R"), x + 200, y)
        v = lerp(m, v, (sample, "A"), sandy, x + 400, y)
        return lerp(m, v, 0.5, (masks, "G"), x + 600, y)

    near = times(m, kind_of_ground(fine, -2000, 700, crowns), 2.0, -1200, 700)
    far = lerp(m, 1.0, times(m, kind_of_ground(coarse, -2000, 1000), 2.0, -1200, 1000), scalar(m, "DetailCoarse", 0.45, -1200, 1150), -1000, 1000)
    detail = lerp(m, 1.0, times(m, near, far, -800, 850), scalar(m, "DetailStrength", 0.8, -800, 1000), -600, 850)
    detail = lerp(m, detail, 1.0, wetness, -500, 850)  # (no grass on water)

    # Shaded as a printed relief map is, on top of whatever the lamp does.
    # (the picture's blue is how squarely the ground faces a low sun in the north-west, shadows and all: the sun's
    # share of the light is warm and the shade's cool, so a lit slope and a shaded one differ in colour too)
    sun = times(m, (relief, "B"), scalar(m, "SunReach", 2.2, -1300, 180), -1100, 100)
    light = plus(m, vector(m, "ShadeColour", (0.43, 0.48, 0.58, 1.0), -1100, 260),
                 times(m, vector(m, "SunColour", (0.63, 0.56, 0.45, 1.0), -1100, 420), sun, -900, 300), -700, 200)
    c = times(m, times(m, c, detail, -400, -400), light, -200, -400)
    # The staff map (art fix 11): from the top of the zoom the land is a sheet of paper, as the staff's own map
    # is. The hills are shaded on it in sepia, as if printed (from the same picture that shades the living
    # ground); each country is tinted by hand, a wash all over and a firm band of colour inside its frontier.
    # "Paper" is nothing until the eye is high, and all of it at the top.
    # Ground that nobody of the player's can see is the same paper at any height (the "Fog" picture says where):
    # the map showing through where the living table is not known. There the paper is a shade darker and warm
    # whoever holds it, the wash is thin, and the holder's colour is carried by the ink: fine slanting lines ruled
    # across it, and the band inside the frontier.
    paper = scalar(m, "Paper", 0.0, 0, -900)
    fog = times(m, (fog_picture, "R"), scalar(m, "FogStrength", 1.0, -2300, -1700), -2100, -1800)
    # (only the staff map is a sheet of paper now: unseen ground keeps its real ground, under the haze below)
    sheeted = paper
    hill = one_in(m, unreal.MaterialExpressionSaturate,
                  plus(m, scalar(m, "PaperShade", 0.6, -600, -1000), times(m, (relief, "B"), scalar(m, "PaperSun", 0.92, -600, -900), -450, -950), -300, -950), -150, -950)
    sheet = lerp(m, vector(m, "PaperInk", (0.2, 0.135, 0.075, 1.0), -300, -1150), vector(m, "PaperColour", (0.74, 0.66, 0.5, 1.0), -300, -1300), hill, 0, -1100)
    tinted = one_in(m, unreal.MaterialExpressionSaturate,
                    plus(m, times(m, scalar(m, "PaperWash", 0.5, -600, -1450), plus(m, boost, 1.0, -600, -1380), -450, -1420),
                         times(m, (tint, "A"), scalar(m, "PaperBand", 0.62, -600, -1620), -450, -1620), -300, -1480), -150, -1480)
    stain_p = times(m, tint_rgb, plus(m, times(m, hill, 0.5, 0, -1380), 0.6, 100, -1380), 150, -1300)
    sheet = lerp(m, sheet, stain_p, tinted, 250, -1200)
    # (and on the paper the waters are drawn in a blue-grey ink)
    sheet = lerp(m, sheet, vector(m, "PaperWater", (0.13, 0.19, 0.27, 1.0), 300, -1750), times(m, waters, scalar(m, "PaperWaters", 0.75, 300, -1650), 450, -1700), 500, -1500)
    # (on the paper, ground nobody of his can see takes a flat darker wash of its own colour: no lines)
    sheet = times(m, sheet, lerp(m, 1.0, scalar(m, "PaperUnseen", 0.76, 500, -1850), fog, 650, -1800), 800, -1500)
    # THE FOG OF WAR AS A HAZE (Brian, 16 November 2026: "i dont like the lines across. can we make it dark and hazy
    # almost where you can see the map"; the art director's brief; ruling 154). A lamp over a dark table: where he
    # sees, the lamp is on; everywhere else the map is still there, in shadow and haze. The real ground stays under
    # it: two thirds as bright, about half its colour drained (so the holder's stain still shows at half), a cold
    # slate cast; and over it a soft veil of wisps, long and low, that drifts slowly one way. The "Fog" picture says
    # where, and is soft at its edge. From high up the wisps fade into a flat veil, so that they never turn to grain.
    grey_weights = node(m, unreal.MaterialExpressionConstant3Vector, 600, -2300, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
    grey = node(m, unreal.MaterialExpressionDotProduct, 800, -2300)
    connect(c, "", grey, "A")
    connect(grey_weights, "", grey, "B")
    drained = lerp(m, c, grey, scalar(m, "HazeDrain", 0.5, 800, -2450), 1000, -2300)
    shaded = times(m, times(m, drained, vector(m, "HazeCast", (0.92, 1.0, 1.07, 1.0), 1000, -2450), 1200, -2300), scalar(m, "HazeDark", 0.67, 1200, -2450), 1400, -2300)
    haze_place = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionWorldPosition, 0, -2900), 200, -2900, r=True, g=True, b=False, a=False)

    # (The art director's review of the haze as built, 16 November 2026.) A wisp is twice as long as it was and a
    # quarter deeper, about 400 by 150 px from where he plays. The broad wisps and the fine travel one way at one pace,
    # about 5 px a second, the fine riding on the broad: at two paces the veil stirred in place and did not travel.
    # Its run from pocket to wisp is drawn in, so that it reads as haze and not as cloud, which is the weather's sign:
    # a floor of haze lies over the pockets ("HazeFloor"), and the wisps rise out of it more gently.
    # (The art director's ruling of 17 November 2026: the haze travelled at the pace set and nobody could see it,
    # because a wisp was 900 px long and smooth along the way it went. The fix is shape, not speed. The wisps are a
    # picture of their own now, "Haze", which the game makes: bands about 400 px long from where he plays, each with a
    # blunt head and a thinning tail, with lumps about every 150 px that ride with the band. They travel about twenty
    # degrees off their own long axis, so that their edges creep across the province lines. The picture's values are
    # spread exactly as the old wisps' were: no more contrast than before. How far the haze has travelled, east and
    # north, the game says from its own clock ("HazeGone"), so that the pace can be set and held by measure.)
    gone = vector(m, "HazeGone", (0.0, 0.0, 0.0, 0.0), -200, -3050)
    haze_east = node_from(m, unreal.MaterialExpressionComponentMask, haze_place, 0, -2700, r=False, g=True, b=False, a=False)
    haze_north = node_from(m, unreal.MaterialExpressionComponentMask, haze_place, 0, -2580, r=True, g=False, b=False, a=False)
    haze_uv = pair(m, times(m, plus(m, haze_east, times(m, (gone, "R"), -1.0, 100, -2800), 250, -2700), 1.0 / 2920.0, 400, -2700),
                   times(m, plus(m, haze_north, times(m, (gone, "G"), -1.0, 100, -2500), 250, -2580), 1.0 / 2920.0, 400, -2580), 550, -2650)
    wisp = lerp(m, scalar(m, "HazeFloor", 0.55, 1650, -3300), 1.0, (picture(m, "Haze", haze_uv, 800, -2650, numbers=True), "R"), 1720, -3250)
    wisp = times(m, lerp(m, scalar(m, "HazeFlat", 0.72, 1650, -3050), wisp, scalar(m, "HazeWispy", 1.0, 1650, -3150), 1800, -2950), scalar(m, "HazeWisps", 0.38, 1800, -3100), 1950, -2950)
    hazed = lerp(m, shaded, vector(m, "HazeColour", (0.21, 0.24, 0.27, 1.0), 1800, -2600), wisp, 2100, -2500)
    c = lerp(m, c, hazed, fog, 2250, -1500)
    c = sky_over(m, c, haze_place, 2600, -7000)
    c = lerp(m, c, sheet, paper, 2400, -600)
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
    # (paper lies flat: the hills are printed on it, not modelled)
    level = node(m, unreal.MaterialExpressionConstant3Vector, 400, 1700, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    # (and water lies flat too)
    normal = one_in(m, unreal.MaterialExpressionNormalize, lerp(m, normal, level, larger(times(m, sheeted, 0.85, 500, 1650), times(m, wetness, 0.9, 500, 1750), 550, 1700), 600, 1550), 750, 1550)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # (water takes the lamp's light as water does, a little: but none of it on the paper)
    shine = times(m, wetness, one_in(m, unreal.MaterialExpressionOneMinus, sheeted, -350, 380), -250, 380)
    to_property(lerp(m, scalar(m, "Roughness", 0.92, -200, 200), scalar(m, "WetRoughness", 0.32, -200, 260), shine, -50, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(lerp(m, scalar(m, "Specular", 0.22, -200, 300), 0.5, shine, -50, 320), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_sea():
    """The sea: its colour from a picture the game paints (pale over the shoals, dark over the deeps), glossy
    like poured resin, with two sets of small ripples sliding across each other so that the lamp glints and moves on it;
    and over that paint a swell, a chop, glints and a wash of foam that move (the moving sea, below)."""
    m = new_material("M_Sea")
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1800, 0)
    water = picture(m, "Water", uv, -1400, 0)
    # A thin white line of surf just off every shore: the picture's alpha is how far offshore a point is (1 at six
    # map units), and the surf lies between about a tenth of a unit and two thirds of one.
    # (how strong it is on each stretch of shore is in a second picture: it comes and goes, and is strongest where
    # the open ocean runs in)
    shore = picture(m, "Shore", uv, -1400, -500, numbers=True)
    # The hour's grading of the water, all of it nothing by day: by moonlight the game greys it (lit by a blue moon,
    # blue water goes cobalt), tints it and lifts the deeps a little.
    weights = node(m, unreal.MaterialExpressionConstant3Vector, -1250, -760, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
    brightness = node(m, unreal.MaterialExpressionDotProduct, -1100, -760)
    connect(water, "RGB", brightness, "A")
    connect(weights, "", brightness, "B")
    greyed = lerp(m, (water, "RGB"), brightness, scalar(m, "Grey", 0.0, -1100, -880), -950, -760)
    graded = plus(m, times(m, greyed, vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -950, -960), -800, -760), vector(m, "Lift", (0.0, 0.0, 0.0, 1.0), -800, -960), -650, -760)
    # THE MOVING SEA (Brian, 16 November 2026: "can we make the ocean and water ripple and move so its not stagment?";
    # the art director's moving-water brief). The chart stays the chart: the colours and the depths he reads stay
    # where they are, and the surface moves. Four thin layers, all from one picture the game makes ("Waves": red a
    # swell's heights, green a chop's, blue scattered flecks, alpha a slow unevenness), read several times at
    # different headings, sizes and speeds, so that nothing is seen to slide or to repeat. Measures are the table's
    # own units: at playing height a pixel is about a unit and a half.
    import math
    SAT, INV, SUB = unreal.MaterialExpressionSaturate, unreal.MaterialExpressionOneMinus, unreal.MaterialExpressionSubtract
    position = node(m, unreal.MaterialExpressionWorldPosition, -3600, 1400)
    flat = node_from(m, unreal.MaterialExpressionComponentMask, position, -3400, 1400, r=True, g=True, b=False, a=False)
    wind = math.degrees(math.atan2(0.9, 0.25))  # (the way the flags fly and the smoke leans)
    offshore = (water, "A")  # (how far offshore: 1 at six map units, which are sixty of the table's)

    def along(angle, length, speed, x, y):
        """Where to read the picture so that it lies along a heading (degrees off the wind), one turn of it every
        `length` units, and slides that way at `speed` units a second."""
        a = math.radians(wind + angle)
        u = node(m, unreal.MaterialExpressionDotProduct, x, y)
        connect(flat, "", u, "A")
        connect(node(m, unreal.MaterialExpressionConstant2Vector, x - 220, y + 80, r=math.cos(a) / length, g=math.sin(a) / length), "", u, "B")
        v = node(m, unreal.MaterialExpressionDotProduct, x, y + 180)
        connect(flat, "", v, "A")
        connect(node(m, unreal.MaterialExpressionConstant2Vector, x - 220, y + 260, r=-math.sin(a) / length, g=math.cos(a) / length), "", v, "B")
        slide = node(m, unreal.MaterialExpressionPanner, x + 440, y, speed_x=-speed / length, speed_y=0.0)
        connect(pair(m, u, v, x + 220, y), "", slide, "Coordinate")
        return slide

    def waves(uv, x, y):
        return picture(m, "Waves", uv, x, y, numbers=True)

    def less(a, b, x, y):
        n = node(m, SUB, x, y)
        connect(*(a if isinstance(a, tuple) else (a, "")), n, "A")
        connect(*(b if isinstance(b, tuple) else (b, "")), n, "B")
        return n

    # 1. The swell and the chop: two fields that travel downwind, each read twice so that its crests are short and
    # broken and never ridges. The long swell is about 30 px from crest to crest and moves 5 to 10 px a second; the
    # chop about 11 px, 2 to 5 px a second, heading some 25 degrees off the swell.
    swell = plus(m, plus(m, (waves(along(0.0, 264.0, 10.0, -2800, 1400), -2000, 1400), "R"), (waves(along(12.0, 330.0, 7.5, -2800, 1850), -2000, 1850), "R"), -1700, 1600), -1.0, -1550, 1600)
    chop = plus(m, plus(m, (waves(along(25.0, 192.0, 5.0, -2800, 2300), -2000, 2300), "G"), (waves(along(7.0, 150.0, 3.5, -2800, 2750), -2000, 2750), "G"), -1700, 2500), -1.0, -1550, 2500)
    wave = plus(m, times(m, swell, scalar(m, "Swell", 1.3, -1550, 1750), -1350, 1600), times(m, chop, scalar(m, "Chop", 0.55, -1550, 2650), -1350, 2500), -1150, 2000)
    crest = one_in(m, SAT, plus(m, times(m, wave, 1.7, -1000, 1900), -0.42, -850, 1900), -700, 1900)
    trough = one_in(m, SAT, times(m, wave, -1.2, -1000, 2100), -700, 2100)
    # (the crests' light fades toward the shore, to about a third of its strength at the land)
    from_shore = lerp(m, 0.33, 1.0, one_in(m, SAT, times(m, offshore, 1.0 / 0.63, -1000, 2300), -850, 2300), -700, 2300)
    heave = times(m, plus(m, times(m, crest, scalar(m, "Crest", 0.34, -700, 1750), -500, 1900), times(m, trough, scalar(m, "Trough", -0.13, -700, 2200), -500, 2100), -350, 2000), from_shore, -200, 2000)
    # 2. The shallows flicker: within about 30 px of land a finer chop crosses the first. A flicker, never a net of lines.
    fine = plus(m, times(m, (waves(along(-40.0, 104.0, 4.0, -2800, 3200), -2000, 3200), "G"), 2.0, -1700, 3200), -1.0, -1550, 3200)
    shelf = one_in(m, INV, one_in(m, SAT, times(m, offshore, 1.0 / 0.73, -1400, 3400), -1250, 3400), -1100, 3400)
    flicker = times(m, times(m, plus(m, times(m, one_in(m, SAT, plus(m, times(m, fine, 1.5, -1400, 3100), -0.1, -1250, 3100), -1100, 3100), 0.3, -950, 3100),
                                     times(m, one_in(m, SAT, times(m, fine, -1.0, -1400, 3250), -1100, 3250), -0.17, -950, 3250), -800, 3150), shelf, -650, 3200),
                    scalar(m, "Shallows", 1.0, -800, 3400), -500, 3200)
    moving = times(m, graded, plus(m, plus(m, heave, flicker, -50, 2400), 1.0, 100, 2400), 250, 1200)
    # 3. Glints: tiny pale flecks that sit only on the chop's crests, where two scatterings of flecks cross (so a
    # spot of water stays lit for about a quarter of a second, and the band twinkles). Thick in one broad band, whose
    # heart the game moves with the eye, and thin everywhere else; they thin out toward the land.
    flecks = times(m, (waves(along(25.0, 96.0, 5.0, -2800, 3700), -2000, 3700), "B"), (waves(along(120.0, 77.0, 7.0, -2800, 4150), -2000, 4150), "B"), -1700, 3900)
    lit = one_in(m, SAT, plus(m, times(m, flecks, 8.0, -1550, 3900), -0.45, -1400, 3900), -1250, 3900)
    # (The review of build 1446: they were about three times too many and spread evenly, like snow. They gather on the
    # crests of the waves he sees, the swell's and the chop's together, and nowhere else.)
    on_crests = one_in(m, SAT, plus(m, times(m, wave, 3.4, -1550, 4100), -0.62, -1400, 4100), -1250, 4100)
    heart = vector(m, "GlintAt", (0.0, 0.0, 0.000006, 0.0), -1900, 4500)  # (where the band's heart lies on the table, and one over its reach squared)
    away = less(flat, node_from(m, unreal.MaterialExpressionComponentMask, heart, -1700, 4500, r=True, g=True, b=False, a=False), -1500, 4500)
    far = node(m, unreal.MaterialExpressionDotProduct, -1350, 4500)
    connect(away, "", far, "A")
    connect(away, "", far, "B")
    band = lerp(m, scalar(m, "GlintElsewhere", 0.1, -1200, 4650), 1.0, one_in(m, SAT, one_in(m, INV, times(m, far, (heart, "B"), -1200, 4500), -1050, 4500), -900, 4500), -750, 4500)
    glint = times(m, times(m, times(m, times(m, lit, on_crests, -1100, 4000), band, -950, 4000), one_in(m, SAT, times(m, offshore, 1.0 / 0.24, -1100, 4250), -950, 4250), -800, 4000),
                  scalar(m, "Glints", 1.0, -950, 4350), -650, 4000)
    glinting = lerp(m, moving, vector(m, "GlintColour", (0.68, 0.83, 0.96, 1.0), 100, 1450), glint, 400, 1200)
    # 4. Foam at the shore: a pale line that hugs the land, about 3 px wide at full strength and gone by about 8 px
    # out. It breathes: every three seconds the wash comes in and ebbs, each stretch of coast at its own moment (the
    # slow unevenness sets the moment), and a second, broken, fainter line rolls in from about 13 px out with each
    # wash. It never goes white.
    slow = waves(times(m, flat, 1.0 / 700.0, -2800, 5000), -2000, 5000)
    phase = plus(m, times(m, node(m, unreal.MaterialExpressionTime, -2000, 5300), 1.0 / 3.0, -1800, 5300), times(m, (slow, "A"), 2.5, -1800, 5100), -1600, 5200)
    wash = plus(m, times(m, one_in(m, unreal.MaterialExpressionSine, phase, -1450, 5200), 0.5, -1300, 5200), 0.5, -1150, 5200)
    inner = lerp(m, 0.02, 0.05, wash, -1000, 5100)
    outer = lerp(m, 0.075, 0.15, wash, -1000, 5300)
    reach = node(m, unreal.MaterialExpressionDivide, -700, 5200)
    connect(less(outer, offshore, -850, 5300), "", reach, "A")
    connect(less(outer, inner, -850, 5100), "", reach, "B")
    line = one_in(m, SAT, reach, -550, 5200)
    strength = times(m, times(m, times(m, lerp(m, 0.72, 1.28, wash, -1000, 5500), scalar(m, "Surf", 0.45, -1000, 5650), -850, 5550), 1.35, -700, 5550), lerp(m, 0.6, 1.0, (shore, "R"), -850, 5750), -550, 5600)
    roll = one_in(m, unreal.MaterialExpressionFrac, plus(m, phase, 0.75, -1450, 5900), -1300, 5900)
    near = one_in(m, SAT, one_in(m, INV, times(m, one_in(m, unreal.MaterialExpressionAbs, less(offshore, lerp(m, 0.32, 0.08, roll, -1150, 5900), -1000, 5900), -850, 5900), 1.0 / 0.035, -700, 5900), -550, 5900), -400, 5900)
    rolling = times(m, times(m, times(m, near, one_in(m, SAT, plus(m, times(m, chop, 2.0, -700, 6100), 0.3, -550, 6100), -400, 6100), -250, 6000),
                             node_from(m, unreal.MaterialExpressionSine, roll, -400, 6250, period=2.0), -100, 6000), 0.2, 50, 6000)
    foam = one_in(m, SAT, times(m, plus(m, line, rolling, 200, 5600), strength, 350, 5600), 500, 5600)
    colour = lerp(m, glinting, vector(m, "SurfColour", (0.46, 0.74, 0.9, 1.0), 400, 1450), foam, 700, 1200)
    # The staff map (art fix 11): from the top of the zoom the sea is pale blue-grey paper, a little darker over the
    # deeps, with the coast drawn in ink and a second line ruled round it a little way out, as a chart has.
    paper = scalar(m, "Paper", 0.0, 0, -900)

    def ruled(at, wide, x, y):
        off = one_in(m, unreal.MaterialExpressionAbs, plus(m, (water, "A"), -at, x, y), x + 150, y)
        return one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, off, -1.0 / wide, x + 300, y), 1.0, x + 450, y), x + 600, y)

    lines = plus(m, ruled(0.03, 0.09, -1000, -1300), times(m, ruled(0.62, 0.1, -1000, -1450), 0.55, -250, -1450), -100, -1350)
    sheet = lerp(m, vector(m, "PaperSea", (0.3, 0.37, 0.37, 1.0), -300, -1100), vector(m, "PaperDeep", (0.24, 0.31, 0.33, 1.0), -300, -1200),
                 one_in(m, unreal.MaterialExpressionSaturate, plus(m, times(m, brightness, -6.0, -500, -1150), 1.0, -400, -1150), -300, -1050), -100, -1100)
    sheet = lerp(m, sheet, vector(m, "PaperSeaInk", (0.1, 0.15, 0.17, 1.0), -100, -1250), one_in(m, unreal.MaterialExpressionSaturate, lines, 0, -1350), 100, -1150)
    colour = lerp(m, sky_over(m, colour, flat, 600, -5200), sheet, paper, 250, -500)
    to_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    first_uv = node(m, unreal.MaterialExpressionPanner, -1400, 400, speed_x=0.011, speed_y=0.006)
    connect(world_uv(m, "RippleTiling", 0.0095, -2200, 400), "", first_uv, "Coordinate")
    second_uv = node(m, unreal.MaterialExpressionPanner, -1400, 700, speed_x=-0.007, speed_y=0.009)
    connect(world_uv(m, "RippleTilingSecond", 0.0061, -2200, 700), "", second_uv, "Coordinate")
    first = picture(m, "Ripples", first_uv, -1100, 400, numbers=True)
    second = picture(m, "Ripples", second_uv, -1100, 700, numbers=True)
    lean = times(m, plus(m, leaning(m, first, "R", "G", -800, 400), leaning(m, second, "B", "A", -800, 700), -200, 550),
                 times(m, scalar(m, "RippleStrength", 0.07, -400, 800), one_in(m, unreal.MaterialExpressionOneMinus, paper, -400, 900), -250, 850), 0, 550)
    up = node(m, unreal.MaterialExpressionConstant, 0, 750, r=1.0)
    normal = one_in(m, unreal.MaterialExpressionNormalize, pair(m, lean, up, 200, 550), 400, 550)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # (paper does not shine)
    to_property(lerp(m, scalar(m, "Roughness", 0.16, -400, 100), 0.92, paper, -200, 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(lerp(m, scalar(m, "Specular", 0.75, -400, 200), 0.2, paper, -200, 200), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


# ---- The ground battle's ground (release 2) -----------------------------------------------------------------------

LAND_TEXTURES = "/Game/Campaign/Textures/Land"


def land_texture(name, cut_out=False):
    """One of the ground's own detail pictures, painted by Tools/land_look/make_ground_textures.py (or, the leaves,
    make_leaf_textures.py) and kept beside it: brought in under /Game/Campaign/Textures/Land as plain numbers (not
    colours), wrapping, kept as BC7. A cut-out picture (the leaves) keeps its leaves in its smaller pictures: each
    made so that as much of it is over the material's clip (a half) as in the full one, so a hedge does not thin to
    twigs with distance."""
    import os
    source = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "land_look", name + ".png")
    if not os.path.isfile(source):
        raise RuntimeError(f"{source} is missing: run Tools/land_look/make_ground_textures.py")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", LAND_TEXTURES)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(f"{LAND_TEXTURES}/{name}")
    if texture is None:
        raise RuntimeError(f"{LAND_TEXTURES}/{name} was not imported")
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    if cut_out:
        try:
            texture.set_editor_property("do_scale_mips_for_alpha_coverage", True)
            texture.set_editor_property("alpha_coverage_thresholds", unreal.Vector4(0.0, 0.0, 0.0, 0.5))
        except Exception as e:  # (an engine without these: the leaves thin a little with distance, nothing worse)
            unreal.log_warning(f"Theater Command: {name}'s smaller pictures not scaled for their cut-out: {e}")
    eal.save_asset(texture.get_path_name(), only_if_is_dirty=False)
    unreal.log(f"Theater Command: wrote {texture.get_path_name()}")
    return texture


_AIR = {}  # (the air's picture, brought in once a run)


def wind_gust(m, x, y, vertex):
    """THE WIND'S GUSTS (release 2; Brian, 23 November: "can we make the grass, trees, and sky more visually realistic
    and epic?"). How hard the wind blows at a point now, 0 (a lull) to 1 (a gust): T_LandCloudWind's green
    (Tools/land_look/make_sky_textures.py), "GustSize" metres a repeat, carried downwind ("WindWay", the world's X and
    Y) at "WindTravel" metres a second on the game's clock, so the gusts run over the field as they do over corn.
    vertex: read for World Position Offset (its smallest picture never taken, as a vertex has no footprint; the point
    where it stands before any offset)."""
    air = _AIR.get("picture") or _AIR.setdefault("picture", land_texture("T_LandCloudWind"))
    where = node(m, unreal.MaterialExpressionWorldPosition, x, y)
    if vertex:
        where.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    metres = times(m, node_from(m, unreal.MaterialExpressionComponentMask, where, x + 150, y, r=True, g=True, b=False, a=False), 0.01, x + 300, y)
    way = node_from(m, unreal.MaterialExpressionComponentMask, vector(m, "WindWay", (0.91, 0.41, 0.0, 0.0), x, y + 120), x + 150, y + 120,
                    r=True, g=True, b=False, a=False)
    run = times(m, times(m, way, scalar(m, "WindTravel", 5.0, x + 150, y + 220), x + 300, y + 150), node(m, unreal.MaterialExpressionTime, x + 300, y + 250), x + 450, y + 150)
    uv = over(m, plus(m, metres, times(m, run, -1.0, x + 600, y + 150), x + 750, y + 50), scalar(m, "GustSize", 150.0, x + 750, y + 200), x + 900, y + 50)
    gust = node(m, unreal.MaterialExpressionTextureSample, x + 1050, y, texture=air, sampler_type=sampler_for(air))
    if vertex:
        gust.set_editor_property("mip_value_mode", unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
        gust.set_editor_property("const_mip_value", 0)
    connect(uv, "", gust, "UVs")
    return (gust, "G"), metres, way


def wind_sway(m, x, y, leaves=False):
    """The World Position Offset of a thing the wind bends: grass, corn, a shrub or a tree, about its foot (the instance's
    own origin; its height read in its own space, so a man or a house never moves, as their materials have none).
    It leans downwind by "Bend" (radians at a full gust; nothing unless C++ says, so the battle without the wind is as
    before) times the day's calm share ("WindCalm") and the gust over it, and springs back and forth a little in the gust
    ("Spring" of it, "SpringHz" a second). A blade leans as a whole from its root; a tree bends more toward its top
    ("BendCurve" 1: the lean grows as its height over "BendHeight" metres). leaves: each card also flutters on its own
    ("Flutter" metres at a gust, about "FlutterHz" a second)."""
    gust, metres, way = wind_gust(m, x, y, True)
    where = node(m, unreal.MaterialExpressionWorldPosition, x, y + 500)
    where.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    own = node(m, unreal.MaterialExpressionTransformPosition, x + 150, y + 500,
               transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
               transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(where, "", own, "")
    high = node(m, unreal.MaterialExpressionMax, x + 450, y + 500)
    _feed(m, high, "A", times(m, node_from(m, unreal.MaterialExpressionComponentMask, own, x + 300, y + 500, r=False, g=False, b=True, a=False), 0.01, x + 400, y + 560))
    _feed(m, high, "B", 0.0)
    reach = times(m, high, lerp(m, 1.0, over(m, high, scalar(m, "BendHeight", 12.0, x + 450, y + 650), x + 600, y + 600),
                                scalar(m, "BendCurve", 0.0, x + 600, y + 700), x + 750, y + 600), x + 900, y + 550)
    time = node(m, unreal.MaterialExpressionTime, x + 1050, y + 300)
    along = node(m, unreal.MaterialExpressionDotProduct, x + 1050, y + 400)
    connect(metres, "", along, "A")
    connect(way, "", along, "B")
    spring = node(m, unreal.MaterialExpressionSine, x + 1350, y + 350, period=1.0)
    # (each copy on its own beat: a tree, a tuft, a sheaf of corn swings out of step with its neighbours; the art
    # director's round 1, 23 November)
    own_beat = node(m, unreal.MaterialExpressionPerInstanceRandom, x + 1050, y + 480)
    connect(plus(m, plus(m, times(m, time, scalar(m, "SpringHz", 0.55, x + 1050, y + 200), x + 1200, y + 300), times(m, along, 0.04, x + 1200, y + 400), x + 1300, y + 350),
                 own_beat, x + 1300, y + 450), "", spring, "")
    push = plus(m, lerp(m, scalar(m, "WindCalm", 0.3, x + 1200, y + 100), 1.0, gust, x + 1350, y + 100),
                times(m, times(m, spring, scalar(m, "Spring", 0.25, x + 1350, y + 450), x + 1500, y + 400), gust, x + 1650, y + 400), x + 1800, y + 200)
    lean = times(m, times(m, times(m, push, scalar(m, "Bend", 0.0, x + 1800, y + 300), x + 1950, y + 250), reach, x + 2100, y + 300), 100.0, x + 2250, y + 300)
    offset = pair(m, times(m, way, lean, x + 2400, y + 250), node(m, unreal.MaterialExpressionConstant, x + 2400, y + 400, r=0.0), x + 2550, y + 300)
    if leaves:
        # (each card on its own: three slow beats out of step, by where its corners stand)
        whole = times(m, node_from(m, unreal.MaterialExpressionComponentMask, where, x + 150, y + 800, r=True, g=True, b=True, a=False), 0.01, x + 300, y + 800)
        beats = []
        for k, (rate, ax, ay, az, share) in enumerate(((1.0, 1.7, 0.9, 2.3, 1.0), (0.83, 0.8, 2.1, 1.3, 0.8), (1.21, 2.6, 1.4, 0.7, 0.6))):
            phase = node(m, unreal.MaterialExpressionDotProduct, x + 450, y + 800 + 120 * k)
            connect(whole, "", phase, "A")
            connect(node(m, unreal.MaterialExpressionConstant3Vector, x + 300, y + 880 + 120 * k, constant=unreal.LinearColor(ax, ay, az, 0.0)), "", phase, "B")
            beat = node(m, unreal.MaterialExpressionSine, x + 900, y + 800 + 120 * k, period=1.0)
            connect(plus(m, times(m, time, times(m, scalar(m, "FlutterHz", 2.2, x + 450, y + 1200), rate, x + 600, y + 1200 + 40 * k), x + 750, y + 800 + 120 * k), phase, x + 800, y + 800 + 120 * k), "", beat, "")
            beats.append(times(m, beat, share, x + 1050, y + 800 + 120 * k))
        shake = pair(m, pair(m, beats[0], beats[1], x + 1200, y + 850), beats[2], x + 1350, y + 900)
        strength = times(m, times(m, scalar(m, "Flutter", 0.0, x + 1350, y + 1000), lerp(m, 0.4, 1.0, gust, x + 1350, y + 1100), x + 1500, y + 1000), 100.0, x + 1650, y + 1000)
        offset = plus(m, offset, times(m, shake, strength, x + 1800, y + 900), x + 2700, y + 500)
    return offset


def windy(name, make):
    """The wind's part of a material that stood before it (M_LandSurface, M_LandLeaves, M_LandStalks): made, or, if
    anything in it goes wrong, told and left out, so the material is built as it was without the wind."""
    import traceback
    try:
        return make()
    except Exception:
        unreal.log_error(f"Theater Command: {name} built without the wind:\n" + traceback.format_exc())
        return None


def wind_sheen(m, x, y, ripe=None):
    """What a gust does to a field's look from afar: the grass and the corn bent over show their paler sides, so each
    gust runs over a field as a light patch and each lull as a darker one, moving with the lean (the same gusts). A
    share about 1, by "Sheen" (nothing unless C++ says). ripe: where the paint is ripe grain (0..1), by "SheenRipe"
    there instead (the art director's round 1: ripe wheat, barley, gaoliang show it most)."""
    gust, _, _ = wind_gust(m, x, y, False)
    amount = scalar(m, "Sheen", 0.0, x + 1050, y + 100)
    if ripe is not None:
        amount = lerp(m, amount, scalar(m, "SheenRipe", 0.0, x + 1050, y + 200), ripe, x + 1200, y + 150)
    return plus(m, times(m, plus(m, gust, -0.5, x + 1200, y), times(m, amount, 2.0, x + 1350, y + 100), x + 1500, y), 1.0, x + 1650, y)



def build_land_surface():
    """THE GROUND BATTLE'S GROUND (release 2; Brian on the first pictures of Normandy: "the graphics for the terrain
    need a lot of work"). The pastures and bare fields read as felt: one flat colour. This is M_MapSurface's ground
    (the same vertex colours, sRGB made linear, the same light on what faces the sky) with the ground's own grain over
    it, for the battle's ground tiles only; the hedges, trees, grass, houses, tanks and men keep M_MapSurface, and the
    campaign map never sees this material.

    - The grain (T_LandGrain, about 2 m a repeat): a grazed sward seen from above where the vertex colour is green, a
      loam's clods and crumbs where it is brown or gold, read at two sizes and two ways round and mixed, so its
      repeat never lines up. It multiplies the colour: strong close by, fading with distance ("DetailFadeStart" to
      "DetailFadeEnd", in metres, down to "DetailFar"), so the far fields stay calm.
    - The lie of the field (T_LandPatch, about 40 m and, turned, about 97 m): value and warmth broadly (dry, yellow
      and paler, or lush and blue-green), and in a pasture the trodden places where the soil shows.
    - Tussocks (T_LandPatch's alpha): the rank, darker, bluer clumps in a pasture, on grass only (bare earth, a
      cornfield's floor and a lane's dust have none), gathered in the picture itself and again where the turned
      reading is lush, missing where it is dry, so they never lie as an even rash.
    - Which way the ground leans (T_LandBump, from the grain's own heights), flattened with distance.
    All of it only where the surface faces up: a steep bank keeps its paint. The mesh's alpha is not read (the ground
    is painted at 255, which M_MapSurface takes as "not metal")."""
    m = new_material("M_LandSurface")
    # (the same usage as M_MapSurface)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("tangent_space_normal", False)  # the lean is worked out in the world, from the mesh's normal
    grain_picture = land_texture("T_LandGrain")
    bump_picture = land_texture("T_LandBump")
    patch_picture = land_texture("T_LandPatch")

    def sample(texture, uv, x, y):
        n = node(m, unreal.MaterialExpressionTextureSample, x, y, texture=texture, sampler_type=sampler_for(texture))
        connect(uv, "", n, "UVs")
        return n

    def over(a, b, x, y):
        n = node(m, unreal.MaterialExpressionDivide, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    def sat(v, x, y):
        return one_in(m, unreal.MaterialExpressionSaturate, v, x, y)

    def mask(source, channel, x, y):
        return node_from(m, unreal.MaterialExpressionComponentMask, source, x, y,
                         r=channel == "R", g=channel == "G", b=channel == "B", a=False)

    def restored(value, by, x, y):
        """Two readings of a grain mixed are flatter than one: pushed back out from its middle (0.5)."""
        return plus(m, times(m, plus(m, value, -0.5, x, y), by, x + 150, y), 0.5, x + 300, y)

    colour = node(m, unreal.MaterialExpressionVertexColor, -2600, 0)
    # The mesh stores its colours the way a picture file does (sRGB); the renderer wants them linear.
    to_linear = node(m, unreal.MaterialExpressionPower, -2400, 0, const_exponent=2.2)
    connect(colour, "", to_linear, "Base")

    # Where a point is on the ground, in metres, both ways round: (north, east) and, turned, (east, north).
    where = node(m, unreal.MaterialExpressionWorldPosition, -3600, 600)
    metres = times(m, node_from(m, unreal.MaterialExpressionComponentMask, where, -3400, 600, r=True, g=True, b=False, a=False), 0.01, -3250, 600)
    turned = pair(m, mask(metres, "G", -3100, 700), mask(metres, "R", -3100, 780), -2950, 740)
    uv_a = times(m, metres, scalar(m, "GrainTiling", 0.5, -2950, 560), -2800, 600)
    uv_b = plus(m, times(m, turned, scalar(m, "GrainTilingTurned", 0.365, -2800, 820), -2650, 740),
                node(m, unreal.MaterialExpressionConstant2Vector, -2800, 900, r=0.31, g=0.57), -2500, 740)
    patch_uv_a = times(m, metres, scalar(m, "PatchTiling", 0.025, -2950, 1100), -2800, 1100)
    patch_uv_b = plus(m, times(m, turned, scalar(m, "PatchTilingTurned", 0.0103, -2800, 1250), -2650, 1200),
                      node(m, unreal.MaterialExpressionConstant2Vector, -2800, 1320, r=0.43, g=0.11), -2500, 1200)
    grain_a = sample(grain_picture, uv_a, -2300, 500)
    grain_b = sample(grain_picture, uv_b, -2300, 700)
    bump_a = sample(bump_picture, uv_a, -2300, 1500)
    bump_b = sample(bump_picture, uv_b, -2300, 1700)
    patch_a = sample(patch_picture, patch_uv_a, -2300, 1050)
    patch_b = sample(patch_picture, patch_uv_b, -2300, 1250)

    # (how much the turned reading of the grain counts here, from the lie of the field's warmth: never one reading
    # alone)
    lead = plus(m, times(m, (patch_a, "G"), 0.4, -2100, 900), 0.3, -1950, 900)
    restore = scalar(m, "GrainRestore", 1.3, -1950, 980)
    grain = restored(lerp(m, (grain_a, "RGB"), (grain_b, "RGB"), lead, -1900, 600), restore, -1750, 600)
    grass_lum = mask(grain, "R", -1300, 500)
    soil_lum = mask(grain, "G", -1300, 600)
    grass_hue = mask(grain, "B", -1300, 700)

    # Only where the surface faces up: a bank's side or a lane's wall keeps its paint.
    skyward = mask(node(m, unreal.MaterialExpressionVertexNormalWS, -2600, -600), "B", -2400, -600)
    up = sat(times(m, plus(m, skyward, -0.55, -2250, -600), 4.0, -2100, -600), -1950, -600)
    # How far off it is, in metres: the grain fades from "DetailFadeStart" to "DetailFadeEnd".
    depth = times(m, node(m, unreal.MaterialExpressionPixelDepth, -2600, -400), 0.01, -2450, -400)
    fade_start = scalar(m, "DetailFadeStart", 12.0, -2450, -320)
    fade_span = plus(m, scalar(m, "DetailFadeEnd", 140.0, -2450, -240), times(m, fade_start, -1.0, -2300, -240), -2150, -240)
    fade = sat(over(plus(m, depth, times(m, fade_start, -1.0, -2300, -320), -2150, -400), fade_span, -2000, -400), -1850, -400)
    strength = times(m, times(m, scalar(m, "DetailGrain", 1.0, -1700, -500), lerp(m, 1.0, scalar(m, "DetailFar", 0.35, -1850, -300), fade, -1700, -400), -1550, -450),
                     up, -1400, -450)

    # Grass or soil, from the vertex colour itself: green over red is a sward (a pasture, a meadow, a lane's grass),
    # red over green is ground (bare earth, a lane's dust, the floor of a cornfield, a stubble).
    green = sat(plus(m, times(m, plus(m, (colour, "G"), times(m, (colour, "R"), -1.0, -2400, -800), -2250, -800), 12.0, -2100, -800), 0.6, -1950, -800), -1800, -800)
    # The trodden places of a pasture, where the soil shows: only on grass, only facing up.
    wear = times(m, times(m, sat(times(m, (patch_a, "B"), scalar(m, "Wear", 1.0, -2100, -1000), -1950, -1000), -1800, -1000), green, -1650, -950), up, -1500, -950)
    grassy = times(m, green, one_in(m, unreal.MaterialExpressionOneMinus, wear, -1500, -850), -1350, -850)

    # The broad lie: value and warmth from the two readings of the patch picture.
    patch = times(m, plus(m, (patch_a, "RGB"), (patch_b, "RGB"), -2000, 1150), 0.5, -1850, 1150)
    patch_value = times(m, plus(m, mask(patch, "R", -1700, 1100), -0.5, -1550, 1100), scalar(m, "PatchValue", 0.8, -1550, 1000), -1400, 1100)
    patch_warm = times(m, plus(m, mask(patch, "G", -1700, 1250), -0.5, -1550, 1250), scalar(m, "PatchWarmth", 1.0, -1550, 1350), -1400, 1250)
    # (snow lies even: its paint near white, its blue high, the broad lie all but gone from it, so the lie's cool
    # patches are not blue-grey blotches over a snowfield (the art director's camouflage, Manchuria in January, 23
    # November); "PatchSnow" how much of it the snow loses)
    snowy = sat(times(m, plus(m, (colour, "B"), -0.7, -1700, 1480), 8.0, -1550, 1480), -1400, 1480)
    unsnowed = plus(m, times(m, snowy, times(m, scalar(m, "PatchSnow", 0.85, -1550, 1560), -1.0, -1400, 1560), -1250, 1500), 1.0, -1100, 1500)
    patch_on = times(m, times(m, scalar(m, "PatchStrength", 1.0, -1400, 1400), up, -1250, 1400), unsnowed, -1100, 1420)
    # Tussocks: the 40 m reading's, and fewer from the turned reading (larger there, a rank patch rather than a clump),
    # thick where the turned reading is lush (its warmth low), none where it is dry; on grass only.
    lush = sat(plus(m, times(m, (patch_b, "G"), -4.0, -1700, 1500), 3.0, -1550, 1500), -1400, 1500)
    clumps = plus(m, (patch_a, "A"), times(m, (patch_b, "A"), scalar(m, "TussockBroad", 0.5, -1700, 1650), -1550, 1600), -1400, 1600)
    tussock = times(m, times(m, sat(times(m, clumps, lush, -1250, 1500), -1150, 1500), grassy, -1050, 1500), patch_on, -950, 1500)
    value_shift = plus(m, times(m, patch_value, patch_on, -1250, 1100),
                       times(m, tussock, scalar(m, "TussockDark", -0.4, -950, 1600), -800, 1550), -1100, 1100)
    warm_shift = plus(m, times(m, patch_warm, patch_on, -1250, 1250),
                      times(m, tussock, scalar(m, "TussockCool", -0.3, -950, 1700), -800, 1650), -950, 1300)
    broad = times(m, plus(m, value_shift, 1.0, -950, 1100),
                  plus(m, times(m, vector(m, "PatchHue", (0.35, 0.1, -0.45, 0.0), -800, 1350), warm_shift, -800, 1250), 1.0, -650, 1250),
                  -650, 1150)

    # The grain: brightness, and its hue (a sward's own; a soil's follows its brightness: a dry clod's top is paler
    # and warmer, a damp parting darker and colder).
    lum = lerp(m, soil_lum, grass_lum, grassy, -1100, 550)
    factor = lerp(m, 1.0, times(m, lum, 2.0, -950, 550), strength, -800, 550)
    hue = lerp(m, times(m, plus(m, soil_lum, -0.5, -1100, 750), scalar(m, "SoilWarmth", 1.6, -1100, 850), -950, 750),
               times(m, plus(m, grass_hue, -0.5, -1100, 650), 2.0, -950, 650), grassy, -800, 700)
    tinted_by = plus(m, times(m, vector(m, "GrainHue", (0.3, 0.1, -0.35, 0.0), -800, 850), times(m, hue, strength, -650, 750), -500, 800), 1.0, -350, 800)

    # The colour: worn to the soil where it is trodden, the field's broad lie, the grain, and (as M_MapSurface) what
    # faces the sky a little paler.
    worn = times(m, wear, scalar(m, "WornMix", 0.6, -2200, -50), -2050, -100)
    base = lerp(m, to_linear, vector(m, "WornColour", (0.17, 0.14, 0.085, 1.0), -2200, -150), worn, -1900, 0)
    base = times(m, times(m, times(m, base, broad, -650, 0), tinted_by, -500, 0), factor, -350, 0)
    lit_top = plus(m, times(m, sat(skyward, -500, -200), scalar(m, "TopLight", 0.26, -500, -120), -350, -200), 1.0, -200, -200)
    # The wind's gusts running over the grass and the corn as a sheen (wind_sheen; nothing unless C++ says "Sheen"):
    # where the paint is a plant's, green or ripe gold (its green well over its blue, against its green), never bare
    # earth, sand, snow or a lane's dust; only where it faces up.
    plant = sat(plus(m, times(m, over(plus(m, (colour, "G"), times(m, (colour, "B"), -1.0, -2400, -2100), -2250, -2100),
                                      plus(m, (colour, "G"), 0.02, -2250, -2000), -2100, -2050), 5.0, -1950, -2050), -1.5, -1800, -2050), -1650, -2050)
    # (ripe grain's gold, its red well over its green; a green sward's red is under its green)
    ripe = sat(times(m, plus(m, over((colour, "R"), plus(m, (colour, "G"), 0.01, -2400, -2300), -2250, -2300), -0.9, -2100, -2300), 3.3, -1950, -2300), -1800, -2300)
    sheen = windy("M_LandSurface", lambda: wind_sheen(m, -3600, -2600, ripe=ripe))
    lit = times(m, base, lit_top, -200, 0)
    if sheen is not None:
        lit = times(m, lit, lerp(m, 1.0, sheen, times(m, plant, up, -1500, -2050), -1350, -2100), -100, -100)
    to_property(lit, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Which way it leans: the sward's lean or the soil's (red and green, or blue and alpha, of the bump picture), each
    # read as the grain is, the turned reading's lean turned back. Added to the mesh's own normal; flattened with
    # distance.
    lean_a = lerp(m, leaning(m, bump_a, "B", "A", -2000, 1500), leaning(m, bump_a, "R", "G", -2000, 1600), grassy, -1400, 1550)
    lean_b = lerp(m, leaning(m, bump_b, "A", "B", -2000, 1750), leaning(m, bump_b, "G", "R", -2000, 1850), grassy, -1400, 1800)
    lean = times(m, lerp(m, lean_a, lean_b, lead, -1250, 1650), restore, -1100, 1650)
    lean = times(m, times(m, lean, scalar(m, "DetailBump", 0.8, -1100, 1750), -950, 1650),
                 times(m, lerp(m, 1.0, scalar(m, "DetailBumpFar", 0.15, -1100, 1900), fade, -950, 1850), up, -800, 1850), -650, 1700)
    flat_lean = pair(m, lean, node(m, unreal.MaterialExpressionConstant, -650, 1850, r=0.0), -500, 1750)
    normal = one_in(m, unreal.MaterialExpressionNormalize, plus(m, node(m, unreal.MaterialExpressionVertexNormalWS, -500, 1600), flat_lean, -350, 1700), -200, 1700)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    to_property(scalar(m, "Roughness", 0.95, -200, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.15, -200, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_land_leaves():
    """THE HEDGES' AND THE TREES' LEAVES (release 2; Brian, 22 November: "i want the grass and bushes and plants to
    look more realistic"). The ground battle's shrubs and trees carry their leaves on flat cards (LandField's LeafCard),
    each a spray of the leaves' picture (T_LandLeaves, Tools/land_look/make_leaf_textures.py): its twigs and leaves cut
    out by the picture's alpha, the gaps between them open to the sky and the light, both faces drawn. The card's
    vertex colour is its paint (sRGB, made linear, as M_MapSurface's), the picture's colour a number about it (0.5 as
    painted: each leaf a little lighter or darker, a pale midrib, the twigs dark). Lit as foliage: what the sun shines
    through from behind glows a little, yellow-green ("Through", "ThroughStrength"). Instanced, as the shrubs and trees
    are. Without it (build_content.py not run since it came) the game draws the old solid sprigs instead, said once."""
    m = new_material("M_LandLeaves")
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    leaves_picture = land_texture("T_LandLeaves", cut_out=True)
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    to_linear = node(m, unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    connect(colour, "", to_linear, "Base")
    leaves = node(m, unreal.MaterialExpressionTextureSample, -900, 300, texture=leaves_picture, sampler_type=sampler_for(leaves_picture))
    # (the picture's numbers: 0.5 as painted)
    painted = times(m, times(m, to_linear, (leaves, "RGB"), -500, 100), scalar(m, "LeafValue", 2.0, -500, 220), -350, 100)
    to_property(painted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    through = times(m, times(m, painted, vector(m, "Through", (0.9, 1.0, 0.45, 1.0), -350, 300), -200, 250),
                    scalar(m, "ThroughStrength", 0.6, -350, 400), -50, 300)
    to_property(through, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    to_property((leaves, "A") if False else node_from(m, unreal.MaterialExpressionComponentMask, (leaves, "A"), -500, 500, r=True, g=False, b=False, a=False),
                "", unreal.MaterialProperty.MP_OPACITY_MASK)
    to_property(scalar(m, "Roughness", 0.75, -200, 600), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.3, -200, 700), "", unreal.MaterialProperty.MP_SPECULAR)
    # (the wind: the tree or the shrub bends as its wood does, M_LandBlades with the same numbers, and each card
    # flutters besides; nothing unless C++ says. Brian, 23 November: grass, trees and sky "more visually realistic")
    sway = windy("M_LandLeaves", lambda: wind_sway(m, -1800, 1000, leaves=True))
    if sway is not None:
        to_property(sway, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    finish(m)


def build_land_stalks():
    """THE CORN NEAR THE LENS, STALK BY STALK (release 2; the art director on cut 4, fix 4: "the near wheat reads as
    paper"). Within 3.5 m of a low lens the corn's bundles are taken away and each clump drawn as its own stalks
    (LandField's Stalks: thin stems, drooping leaves, an ear of crossed strips, the poppies' cups), painted by vertex
    colour (sRGB, made linear, as M_LandLeaves) times "Value", lit as foliage (what the sun shines through glows a
    little, "Through", "ThroughStrength"). Both faces are in the mesh, so it is drawn one-sided. It is faded in and out
    by dither (smoothed by the temporal anti-aliasing): in from "NearFrom" over "NearSpan" metres of depth, so no stalk
    stands across the lens itself, and out from "FarTo" less "FarSpan" to "FarTo" metres along the ground, where the
    bundles take over (the C++ sets all four, LandBattle::LoadInks). Its shadow ignores the fades. Instanced, as the
    clutter is. Without it the corn keeps its bundles to the lens, said once."""
    m = new_material("M_LandStalks")
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", False)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    to_linear = node(m, unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    connect(colour, "", to_linear, "Base")
    painted = times(m, to_linear, scalar(m, "Value", 1.0, -700, 150), -500, 50)
    # (the wind, as the corn's bundles beyond: M_LandBlades' numbers; nothing unless C++ says)
    sheen = windy("M_LandStalks", lambda: wind_sheen(m, -2400, 1400))
    sway = windy("M_LandStalks", lambda: wind_sway(m, -2400, 1800)) if sheen is not None else None
    if sheen is not None and sway is not None:
        painted = times(m, painted, sheen, -450, 0)
        to_property(sway, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    to_property(painted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    through = times(m, times(m, painted, vector(m, "Through", (0.9, 1.0, 0.45, 1.0), -500, 250), -350, 200),
                    scalar(m, "ThroughStrength", 0.5, -500, 350), -200, 250)
    to_property(through, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    def over(a, b, x, y):
        n = node(m, unreal.MaterialExpressionDivide, x, y)
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        return n

    def sat(v, x, y):
        return one_in(m, unreal.MaterialExpressionSaturate, v, x, y)

    # In by the depth from the lens (metres), out by the distance along the ground from it.
    depth = times(m, node(m, unreal.MaterialExpressionPixelDepth, -1600, 500), 0.01, -1450, 500)
    near_in = sat(over(plus(m, depth, times(m, scalar(m, "NearFrom", 0.6, -1450, 580), -1.0, -1300, 580), -1150, 500),
                       scalar(m, "NearSpan", 0.3, -1150, 600), -1000, 500), -850, 500)
    here = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionWorldPosition, -1750, 750), -1600, 750,
                     r=True, g=True, b=False, a=False)
    eye = node_from(m, unreal.MaterialExpressionComponentMask, node(m, unreal.MaterialExpressionCameraPositionWS, -1750, 850), -1600, 850,
                    r=True, g=True, b=False, a=False)
    apart = node(m, unreal.MaterialExpressionDistance, -1450, 800)
    connect(here, "", apart, "A")
    connect(eye, "", apart, "B")
    along = times(m, apart, 0.01, -1300, 800)
    far_out = sat(over(plus(m, scalar(m, "FarTo", 5.0, -1300, 900), times(m, along, -1.0, -1150, 800), -1000, 850),
                       scalar(m, "FarSpan", 1.5, -1000, 950), -850, 850), -700, 850)
    shown = node(m, unreal.MaterialExpressionMin, -550, 650)
    connect(near_in, "", shown, "A")
    connect(far_out, "", shown, "B")
    # (the engine's DitherTemporalAA is a material function, not a node: called, its "Alpha Threshold" the share shown)
    dither = node(m, unreal.MaterialExpressionMaterialFunctionCall, -400, 650)
    dither.set_material_function(unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
    connect(shown, "", dither, "Alpha Threshold")
    # (the shadow ignores the fades: the stalks' shadows fall whole, and none flickers with the dither; the node shown
    # as ShadowPassSwitch is MaterialExpressionShadowReplace)
    cast = node(m, unreal.MaterialExpressionShadowReplace, -250, 650)
    connect(dither, "", cast, "Default")
    connect(scalar(m, "ShadowMask", 1.0, -400, 780), "", cast, "Shadow")
    to_property(cast, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    to_property(scalar(m, "Roughness", 0.8, -200, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_property(scalar(m, "Specular", 0.3, -200, 500), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_land_stalks_or_tell():
    """M_LandStalks is the ground battle's alone, and the corn keeps its bundles to the lens without it: whatever goes
    wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_land_stalks()
    except Exception:
        unreal.log_error("Theater Command: M_LandStalks was not built (the corn keeps its bundles to the lens):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandStalks"):
                eal.delete_asset(f"{MATERIALS}/M_LandStalks")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandStalks could not be removed:\n" + traceback.format_exc())


def build_land_shade():
    """THE SHADE ON THE GROUND UNDER A TANK, A GUN OR A MAN (release 2; the art director on cut 4, fix 3: "a soft
    footprint shade under every vehicle and wreck"). The light that the sky and the bounce would have given the ground
    under a hull, which the sun's shadow alone does not take away: a flat mesh laid just over the ground (LandShade.h),
    multiplying what is drawn behind it (modulate), never lighting anything. Its vertex colour's red is the share of the
    light left there as the eye would measure it (0.35 under a tank's belly, 1.0 where it ends), made linear here;
    "Strength" (C++, -TheaterLandFootShade) takes it from none (0) to all (1). Unlit, both faces, instanced (one mesh a
    kind of tank, gun or man). Without it the battle draws no footprint shade, said once."""
    m = new_material("M_LandShade")
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MODULATE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    to_linear = node(m, unreal.MaterialExpressionPower, -650, 0, const_exponent=2.2)
    connect(colour, "R", to_linear, "Base")
    left = lerp(m, 1.0, to_linear, scalar(m, "Strength", 1.0, -650, 200), -400, 0)
    to_property(left, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_land_shade_or_tell():
    """M_LandShade is the ground battle's alone, and the battle draws no footprint shade without it: whatever goes
    wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_land_shade()
    except Exception:
        unreal.log_error("Theater Command: M_LandShade was not built (the ground battle draws no footprint shade):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandShade"):
                eal.delete_asset(f"{MATERIALS}/M_LandShade")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandShade could not be removed:\n" + traceback.format_exc())


def build_land_steel_or_tell():
    """TK1: M_LandSteel (M_MapSurface with the wrecks' soot, for the far tanks, guns and lorries drawn as instances) is
    the ground battle's alone, and the battle falls back to the near meshes' ink without it (a far wreck then shows her
    paint unburnt): whatever goes wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_map_surface("M_LandSteel", soot=True)
    except Exception:
        unreal.log_error("Theater Command: M_LandSteel was not built (far wrecks show their paint unburnt):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandSteel"):
                eal.delete_asset(f"{MATERIALS}/M_LandSteel")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandSteel could not be removed:\n" + traceback.format_exc())


def build_land_leaves_or_tell():
    """M_LandLeaves is the ground battle's alone, and the battle falls back to its solid sprigs without it: whatever goes
    wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_land_leaves()
    except Exception:
        unreal.log_error("Theater Command: M_LandLeaves was not built (the ground battle draws its solid sprigs):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandLeaves"):
                eal.delete_asset(f"{MATERIALS}/M_LandLeaves")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandLeaves could not be removed:\n" + traceback.format_exc())


def build_land_cloud_shade():
    """THE CLOUDS' SHADOWS ON THE FIELD (release 2; Brian, 23 November: "can we make the grass, trees, and sky more
    visually realistic and epic?"; the art director's round 1 the same day). The light function on the battle's sun: on
    a fair day the cumulus' shadows lie on the field, on the hedges, the trees, the tanks and the men alike (it takes
    the sun's light away, not the paint), and slide over it as the wind carries the clouds. Its number is the share of
    the sun let through: 1 in the open, "Dark" under a cloud.

    Where a point's sun ray ("SunWay", the way to the sun in the world) meets the clouds' base ("CloudBase", metres of
    the world's height), carried back by the clouds' drift ("Drift", metres in the world's X and Y, the same carry as
    the clouds drawn), T_LandCloudWind is read, "ShadeSize" metres a repeat, moved by "Anchor" (repeats: the C++ lays
    the picture's clear disc over the field's middle at the start). Its red is how far inside a cloud's shadow the point
    is ("Reach" metres either side of 0.5; make_sky_textures.py), so the edge is blurred over "Feather" metres the
    same all round every cloud. Its blue is the cloud's place in the day's order: only clouds under "Cover" /
    "FullCover" (the share the picture's clouds shade in all, make_sky_textures.py's FULL_COVER) are drawn, so a day of
    less cover has fewer clouds, not smaller ones. "Strength" takes it from none (0, the default: the sun as before) to
    all (1)."""
    m = new_material("M_LandCloudShade")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    air = _AIR.get("picture") or _AIR.setdefault("picture", land_texture("T_LandCloudWind"))
    where = times(m, node(m, unreal.MaterialExpressionWorldPosition, -2400, 0), 0.01, -2250, 0)
    sun = vector(m, "SunWay", (0.3, 0.5, 0.8, 0.0), -2400, 200)
    sun_up = node(m, unreal.MaterialExpressionMax, -2100, 300)
    _feed(m, sun_up, "A", (sun, "B"))
    _feed(m, sun_up, "B", 0.15)
    rise = plus(m, scalar(m, "CloudBase", 900.0, -2250, 400), times(m, node_from(m, unreal.MaterialExpressionComponentMask, where, -2100, 450, r=False, g=False, b=True, a=False), -1.0, -1950, 450), -1800, 400)
    climb = over(m, rise, sun_up, -1650, 350)
    sun_flat = node_from(m, unreal.MaterialExpressionComponentMask, sun, -2100, 200, r=True, g=True, b=False, a=False)
    at = plus(m, plus(m, node_from(m, unreal.MaterialExpressionComponentMask, where, -2100, 0, r=True, g=True, b=False, a=False), times(m, sun_flat, climb, -1500, 250), -1350, 100),
              times(m, node_from(m, unreal.MaterialExpressionComponentMask, vector(m, "Drift", (0.0, 0.0, 0.0, 0.0), -1650, 550), -1500, 550, r=True, g=True, b=False, a=False), -1.0, -1350, 550), -1200, 200)
    anchor = node_from(m, unreal.MaterialExpressionComponentMask, vector(m, "Anchor", (0.0, 0.0, 0.0, 0.0), -1200, 650), -1050, 650, r=True, g=True, b=False, a=False)
    shade = node(m, unreal.MaterialExpressionTextureSample, -750, 200, texture=air, sampler_type=sampler_for(air))
    connect(plus(m, over(m, at, scalar(m, "ShadeSize", 4000.0, -1200, 350), -1050, 250), anchor, -900, 300), "", shade, "UVs")
    # (how far inside, in metres; the edge's blur; the clouds the day's cover keeps)
    inside = times(m, plus(m, (shade, "R"), -0.5, -600, 200), times(m, scalar(m, "Reach", 400.0, -750, 450), 2.0, -600, 450), -450, 250)
    feather = scalar(m, "Feather", 30.0, -600, 550)
    ramp = one_in(m, unreal.MaterialExpressionSaturate, plus(m, over(m, inside, feather, -300, 300), 0.5, -150, 300), 0, 300)
    smooth = times(m, times(m, ramp, ramp, 150, 300), plus(m, 3.0, times(m, ramp, -2.0, 150, 450), 300, 450), 450, 350)
    keep_to = over(m, scalar(m, "Cover", 0.3, -600, 700), scalar(m, "FullCover", 0.41, -600, 800), -450, 750)
    kept = one_in(m, unreal.MaterialExpressionSaturate, times(m, plus(m, keep_to, times(m, (shade, "B"), -1.0, -450, 850), -300, 800), 255.0, -150, 800), 0, 800)
    lit = lerp(m, 1.0, scalar(m, "Dark", 0.2, 450, 500), times(m, smooth, kept, 600, 600), 750, 450)
    to_property(lerp(m, 1.0, lit, scalar(m, "Strength", 0.0, 750, 650), 900, 450), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_land_blades_or_tell():
    """M_LandBlades (M_MapSurface with the wind's sway and sheen) is the ground battle's alone, and the battle draws its
    grass, corn and trees on M_MapSurface without it (still in the wind, said once): whatever goes wrong in it is told,
    and the rest of the content is built as before."""
    import traceback
    try:
        build_map_surface("M_LandBlades", sway=True)
    except Exception:
        unreal.log_error("Theater Command: M_LandBlades was not built (the grass and trees stand still in the wind):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandBlades"):
                eal.delete_asset(f"{MATERIALS}/M_LandBlades")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandBlades could not be removed:\n" + traceback.format_exc())


def build_land_cloud_shade_or_tell():
    """M_LandCloudShade is the ground battle's alone, and the battle lays no cloud shadows without it: whatever goes
    wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_land_cloud_shade()
    except Exception:
        unreal.log_error("Theater Command: M_LandCloudShade was not built (no cloud shadows on the ground battle's field):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandCloudShade"):
                eal.delete_asset(f"{MATERIALS}/M_LandCloudShade")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandCloudShade could not be removed:\n" + traceback.format_exc())


def build_land_surface_or_tell():
    """M_LandSurface is the ground battle's alone, and the battle falls back to M_MapSurface without it: whatever goes
    wrong in it is told, and the rest of the content is built as before."""
    import traceback
    try:
        build_land_surface()
    except Exception:
        unreal.log_error("Theater Command: M_LandSurface was not built (the ground battle falls back to M_MapSurface):\n" + traceback.format_exc())
        try:
            if eal.does_asset_exist(f"{MATERIALS}/M_LandSurface"):
                eal.delete_asset(f"{MATERIALS}/M_LandSurface")
        except Exception:
            unreal.log_error("Theater Command: and the half-built M_LandSurface could not be removed:\n" + traceback.format_exc())


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


def build_flak_smoke():
    """A burst of flak as hanging smoke, from the art director's painted sheet: the picture's own colour and its own
    see-through edge, times the mesh's colour (the light of the hour) and its alpha (how much of the smoke is left:
    a half is the smoke as painted, more than a half is thicker). Light of its own, not shaded, seen from both sides."""
    m = new_material("M_FlakSmoke")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    picture = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0,
                   parameter_name="Sheet",
                   texture=unreal.load_asset("/Engine/EngineResources/DefaultTexture"),
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    colour = node(m, unreal.MaterialExpressionVertexColor, -700, 320)
    lit = node(m, unreal.MaterialExpressionMultiply, -400, 0)
    connect(picture, "RGB", lit, "A")
    connect(colour, "", lit, "B")
    to_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    left = node(m, unreal.MaterialExpressionMultiply, -400, 320)
    connect(picture, "A", left, "A")
    connect(colour, "A", left, "B")
    doubled = node(m, unreal.MaterialExpressionMultiply, -200, 320)
    connect(left, "", doubled, "A")
    connect(scalar(m, "Thick", 2.0, -400, 480), "", doubled, "B")
    kept = node(m, unreal.MaterialExpressionSaturate, 0, 320)
    connect(doubled, "", kept, "")
    to_property(kept, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def build_beam():
    """A searchlight's beam, its lamp, and the bomber it holds: light added over the scene, never darkening anything.
    The mesh's own colour is the light's colour and its alpha how strong it is there."""
    m = new_material("M_Beam")
    # (drawn after the lens's blur: from close by the ground is out of focus, and a beam blurred with it was a smear)
    m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    colour = node(m, unreal.MaterialExpressionVertexColor, -700, 0)
    lit = node(m, unreal.MaterialExpressionMultiply, -450, 0)
    connect(colour, "", lit, "A")
    connect(colour, "A", lit, "B")
    strong = node(m, unreal.MaterialExpressionMultiply, -250, 0)
    connect(lit, "", strong, "A")
    connect(scalar(m, "Strength", 1.0, -450, 200), "", strong, "B")
    to_property(strong, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_glow():
    """Firelight on the table (the red glow of a burning town by night): light added over what is there, never
    darkening anything. The mesh's own colour is the light's colour and its alpha how strong it is there. It is drawn
    in the ordinary translucent pass, with the scene's own blur, so that smoke and signs can be drawn over it."""
    m = new_material("M_Glow")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    colour = node(m, unreal.MaterialExpressionVertexColor, -700, 0)
    lit = node(m, unreal.MaterialExpressionMultiply, -450, 0)
    connect(colour, "", lit, "A")
    connect(colour, "A", lit, "B")
    strong = node(m, unreal.MaterialExpressionMultiply, -250, 0)
    connect(lit, "", strong, "A")
    connect(scalar(m, "Strength", 1.0, -450, 200), "", strong, "B")
    to_property(strong, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_flak_lit():
    """Flak smoke inside a searchlight's beam: the painted shape again, as light added where the smoke is."""
    m = new_material("M_FlakLit")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    picture = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0,
                   parameter_name="Sheet",
                   texture=unreal.load_asset("/Engine/EngineResources/DefaultTexture"),
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    colour = node(m, unreal.MaterialExpressionVertexColor, -700, 320)
    lit = node(m, unreal.MaterialExpressionMultiply, -450, 160)
    connect(colour, "", lit, "A")
    connect(colour, "A", lit, "B")
    shaped = node(m, unreal.MaterialExpressionMultiply, -250, 80)
    connect(lit, "", shaped, "A")
    connect(picture, "A", shaped, "B")
    to_property(shaped, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


# Each asset is left alone if it is already there, so adding one does not rewrite the others.
# Set TC_REBUILD=all to remake everything, or to a material's name (or several, with commas between) to remake those.
import os
REBUILD = os.environ.get("TC_REBUILD", "")
# (M_LandSurface, the ground battle's ground, comes after M_MapSurface and can never stop the rest: see
# build_land_surface_or_tell. After the pictures are painted again, TC_REBUILD=M_LandSurface brings them in again.
# M_LandLeaves, its hedges' and trees' leaves, the same: TC_REBUILD=M_LandLeaves after make_leaf_textures.py.
# M_LandShade, the footprint shade under tanks, guns and men, the same; M_LandStalks, the corn's stalks near the lens,
# the same. M_LandSteel, the far vehicles' instanced pieces with their soot (TK1), the same: TC_REBUILD=M_LandSteel
# after a change to M_MapSurface. M_LandCrowd, the mid and far men as posed instances (SO1), the same:
# TC_REBUILD=M_LandCrowd after a change to M_MapSurface.)
# (GS6: M_CloudTurned, the clouds turned on the graphics chip, the same: TC_REBUILD=M_CloudTurned after a change to
# Battle/AirSkyChip.inl, whose text its Custom node holds, or to M_Cloud's pixels, which it repeats.)
# (The wind and the clouds' shadows, release 2, 23 November: M_LandBlades, the grass, corn and trees the wind bends, and
# M_LandCloudShade, the clouds' shadows on the field, the same; they and M_LandSurface, M_LandLeaves and M_LandStalks read
# Tools/land_look/T_LandCloudWind.png: TC_REBUILD=M_LandBlades,M_LandCloudShade,M_LandSurface,M_LandLeaves,M_LandStalks
# once, and again after make_sky_textures.py.)
for name, build in (("M_MapSurface", build_map_surface), ("M_LandSteel", build_land_steel_or_tell), ("M_LandCrowd", build_land_crowd_or_tell), ("M_LandSurface", build_land_surface_or_tell), ("M_LandLeaves", build_land_leaves_or_tell), ("M_LandShade", build_land_shade_or_tell), ("M_LandStalks", build_land_stalks_or_tell), ("M_LandBlades", build_land_blades_or_tell), ("M_LandCloudShade", build_land_cloud_shade_or_tell), ("M_Solid", build_solid), ("M_Highlight", build_highlight), ("M_CloudTurned", build_cloud_turned_or_tell), ("M_Flag", build_flag), ("M_Land", build_land), ("M_Sea", build_sea), ("M_Wake", build_wake), ("M_FlakSmoke", build_flak_smoke), ("M_Beam", build_beam), ("M_FlakLit", build_flak_lit), ("M_Glow", build_glow), ("M_Cloud", build_cloud)):
    if REBUILD == "all" or name in REBUILD.split(",") or not eal.does_asset_exist(f"{MATERIALS}/{name}"):
        build()
    else:
        unreal.log(f"Theater Command: {MATERIALS}/{name} already exists, left alone")
def build_battle_puffs(name="M_BattlePuffs", soft=False):
    """The battle effects kit's smoke, dust, fire and flashes (Effects/BattleEffects): M_Highlight's light that is not
    shaded, see-through by the mesh's own alpha, "Brightness" times its colour (picture.lit_gain, so the hottest flames
    burn past white) and "Tint", but the colour is stored as a picture file stores it (sRGB) and made linear here, as
    M_MapSurface's. In plain bytes, a charcoal smoke written four times darker was one to four steps a channel, so its
    red, green and blue rounded apart: specks of red and blue in dark smoke at dusk, and a big far piece of dark earth
    read olive over grass (the art director, 22 November). The kit writes its bytes to suit when it finds this
    material (TheaterFx::Battle::SetSrgbBytes); without it, M_Highlight and plain bytes, as before.
    soft (M_BattlePuffsSoft, the land battle's soft contact): every piece also fades where it comes within FadeCm
    (world units) in front of the ground or a hull, by the scene's depth there less its own, so a burning wreck's flame
    meets the hull softly and is never cut along a straight line (the art director's cut 5, still 07). The scene puts
    it on the kit's mesh with FBattleEffects::SetSoftContact; the kit lays its lying pieces nearer the eye to match.
    Its own material, so the scenes without it pay nothing for the depth read."""
    m = new_material(name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    colour = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    to_linear = node(m, unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    connect(colour, "", to_linear, "Base")
    glow = node(m, unreal.MaterialExpressionMultiply, -450, 0)
    connect(to_linear, "", glow, "A")
    connect(scalar(m, "Brightness", 1.0, -700, 200), "", glow, "B")
    tinted = node(m, unreal.MaterialExpressionMultiply, -250, 0)
    connect(glow, "", tinted, "A")
    connect(vector(m, "Tint", (1.0, 1.0, 1.0, 1.0), -450, -160), "", tinted, "B")
    to_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    faded = node(m, unreal.MaterialExpressionMultiply, -450, 200)
    connect(colour, "A", faded, "A")
    connect(scalar(m, "Opacity", 1.0, -700, 320), "", faded, "B")
    if soft:
        # (how far in front of the opaque world this pixel of the piece is, as a share of FadeCm, held 0 to 1)
        gap = node(m, unreal.MaterialExpressionSubtract, -700, 480)
        connect(node(m, unreal.MaterialExpressionSceneDepth, -900, 440), "", gap, "A")
        connect(node(m, unreal.MaterialExpressionPixelDepth, -900, 540), "", gap, "B")
        share = node(m, unreal.MaterialExpressionDivide, -550, 480)
        connect(gap, "", share, "A")
        connect(scalar(m, "FadeCm", 10.0, -700, 600), "", share, "B")
        touch = one_in(m, unreal.MaterialExpressionSaturate, share, -400, 480)
        faded = times(m, faded, touch, -250, 300)
    to_property(faded, "", unreal.MaterialProperty.MP_OPACITY)
    # (the pieces mark where they are on the screen, in the custom depth, wherever they are a tenth seen or more, so
    # M_FireSpread knows the kit's own pixels from the field's. Nothing is marked unless the scene asks for the fire's
    # spread: FBattleEffects::SetFireSpread turns the mesh's custom depth on)
    m.set_editor_property("allow_translucent_custom_depth_writes", True)
    m.set_editor_property("opacity_mask_clip_value", 0.1)
    finish(m)


# The fire's spread, after the scene's grade: in the post-process input (the picture as the screen shows it), a pixel
# the kit marked and the opaque world does not hide has its colour spread from its own grey by Spread when it is fire's
# (orange to yellow, deep in colour, or bright and a little coloured) or a bluish-green light (bright and coloured: a
# Soviet tracer, which the grade leaves grey-green; red is fire's hue already, a field's greens are yellow-green, and
# blue and violet are the sky's, all left alone). The whitest are left as they are (a white-hot core stays as ruled). Warm raises green on fire's orange only
# (on yellow it went lime, on the sky cyan: the pre-check). All else is left as it was. "heat" feathers it, so there
# is no seam at a flame's edge. tools/battle-fx/land_grade.py is the same sums.
FIRE_SPREAD_HLSL = """
float3 c = saturate(C.rgb);
if (!(Marked.r < MarkFar && Marked.r <= Solid.r * 1.001 + 1.0) || Spread <= 1.0) return C.rgb;
float mx = max(c.r, max(c.g, c.b));
float mn = min(c.r, min(c.g, c.b));
float s = (mx - mn) / max(mx, 1e-4);
float hx = 2.0 * c.r - c.g - c.b;
float hy = 1.7320508 * (c.g - c.b);
float h = (abs(hx) + abs(hy) > 1e-5) ? degrees(atan2(hy, hx)) : 0.0;
float hue = smoothstep(-25.0, -12.0, h) * (1.0 - smoothstep(55.0, 65.0, h));
float y = dot(c, float3(0.2126, 0.7152, 0.0722));
float top = 1.0 - smoothstep(0.85, 0.95, y);
float fire = hue * max(smoothstep(0.30, 0.45, s), smoothstep(0.70, 0.85, y) * smoothstep(0.08, 0.18, s) * top);
float green = smoothstep(95.0, 108.0, h) * (1.0 - smoothstep(160.0, 175.0, h));
float lit = green * smoothstep(0.66, 0.78, y) * smoothstep(0.12, 0.20, s) * top;
float dim = smoothstep(0.01, 0.04, y);
float heat = max(fire, lit) * dim;
float3 o = y + (c - y) * (1.0 + (Spread - 1.0) * heat);
o.g *= 1.0 + (Warm - 1.0) * fire * dim * (1.0 - smoothstep(32.0, 42.0, h));
return saturate(o);
"""


def build_fire_spread():
    """Fire keeps its orange under a scene's grade (the art director, 22 November: the land battle's grade, made for
    its fields, drained a burning wreck's flame to salmon and peach). A post-process drawn after the tonemapper, so it
    works on the picture as the screen shows it: the kit's pixels are known by the custom depth its mesh writes
    (M_BattlePuffs), and of those only the fire-coloured are spread (FIRE_SPREAD_HLSL). Smoke, dust, earth and the
    field behind a thin flame stay as the grade left them. Added to a scene's lens by FBattleEffects::SetFireSpread."""
    m = new_material("M_FireSpread")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    after = getattr(unreal.BlendableLocation, "BL_SCENE_COLOR_AFTER_TONEMAPPING", None)
    if after is None:
        after = unreal.BlendableLocation.BL_AFTER_TONEMAPPING  # (its name before 5.4; hidden, and not in python, since)
    m.set_editor_property("blendable_location", after)
    spread = node(m, unreal.MaterialExpressionCustom, -200, 0,
                  code=FIRE_SPREAD_HLSL,
                  description="FireSpread",
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ("C", "Marked", "Solid", "Spread", "Warm", "MarkFar")
    pins = []
    for name in names:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    spread.set_editor_property("inputs", pins)
    for name, which, y in (("C", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, 0),
                           ("Marked", unreal.SceneTextureId.PPI_CUSTOM_DEPTH, 160),
                           ("Solid", unreal.SceneTextureId.PPI_SCENE_DEPTH, 320)):
        texture = node(m, unreal.MaterialExpressionSceneTexture, -600, y, scene_texture_id=which)
        connect(texture, "Color", spread, name)
    # (MarkFar: a custom depth this far or more is a pixel no piece marked; the cleared value is far beyond any battle)
    for name, default, y in (("Spread", 2.0, 480), ("Warm", 1.2, 600), ("MarkFar", 1.0e8, 720)):
        connect(scalar(m, name, default, -600, y), "", spread, name)
    to_property(spread, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


# (M_BattlePuffs, the battle effects kit's: built when missing, or with TC_REBUILD=M_BattlePuffs or all. Without it the
# kit draws with M_Highlight in plain bytes, as before)
if REBUILD == "all" or "M_BattlePuffs" in REBUILD.split(",") or not eal.does_asset_exist(f"{MATERIALS}/M_BattlePuffs"):
    build_battle_puffs()
else:
    unreal.log(f"Theater Command: {MATERIALS}/M_BattlePuffs already exists, left alone")
# (M_BattlePuffsSoft, the land battle's soft contact (FBattleEffects::SetSoftContact): M_BattlePuffs, fading where it
# meets the ground or a hull. The same: built when missing, or with TC_REBUILD=M_BattlePuffsSoft or all)
if REBUILD == "all" or "M_BattlePuffsSoft" in REBUILD.split(",") or not eal.does_asset_exist(f"{MATERIALS}/M_BattlePuffsSoft"):
    build_battle_puffs("M_BattlePuffsSoft", soft=True)
else:
    unreal.log(f"Theater Command: {MATERIALS}/M_BattlePuffsSoft already exists, left alone")
# (M_FireSpread, the same: built when missing, or with TC_REBUILD=M_FireSpread or all. An M_BattlePuffs made before it
# came does not mark its pixels: TC_REBUILD=M_BattlePuffs,M_FireSpread once)
if REBUILD == "all" or "M_FireSpread" in REBUILD.split(",") or not eal.does_asset_exist(f"{MATERIALS}/M_FireSpread"):
    build_fire_spread()
else:
    unreal.log(f"Theater Command: {MATERIALS}/M_FireSpread already exists, left alone")
build_level()
unreal.log("Theater Command: content built")
