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
    sketched = times(m, vector(m, "DrawnTone", (1.0, 0.92, 0.76, 1.0), -300, -1150), light2, -150, -1050)
    tinted = lerp(m, tinted, lerp(m, tinted, sketched, 0.55, 0, -950), drawn, 150, -500)
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
    connect(node(m, unreal.MaterialExpressionVertexNormalWS, -1100, -2100), "", skyward, "")
    lit_top = plus(m, 1.0, times(m, times(m, one_in(m, unreal.MaterialExpressionSaturate, skyward, -750, -2100), scalar(m, "TopLight", 0.26, -750, -2000), -600, -2050),
                                 times(m, metal, wear, -600, -2150), -450, -2100), -300, -2100)
    weathered = times(m, times(m, tinted, lerp(m, 1.0, plus(m, times(m, (blot, "R"), 0.42, -350, -1500), 0.8, -200, -1500),
                                               times(m, wear, plus(m, times(m, metal, 0.5, 100, -1450), 0.5, 200, -1450), 300, -1450), 450, -1500), 600, -900), lit_top, 700, -1000)
    tinted = lerp(m, weathered, vector(m, "ChipColour", (0.13, 0.125, 0.12, 1.0), 450, -1300), chipped, 750, -900)
    tinted = times(m, tinted, lerp(m, 1.0, vector(m, "WakeTone", (1.0, 1.0, 1.0, 1.0), 600, -1250), on_water, 800, -1200), 900, -950)
    to_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_property(times(m, chipped, 0.85, 750, -700), "", unreal.MaterialProperty.MP_METALLIC)

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
    home = one_in(m, unreal.MaterialExpressionFloor, grid, -700, 2500)
    cell = one_in(m, unreal.MaterialExpressionFrac, grid, -700, 2620)
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
    grain = lerp(m, lerp(m, vector(m, "FieldMean", (1.5, 1.3, 0.9, 1.0), 3750, 3500), vector(m, "CoolFieldMean", (0.98, 0.98, 0.94, 1.0), 3750, 3560), cool_paint, 3800, 3530), grain, lerp(m, 0.33, 1.0, work, 3850, 3500), 3950, 3400)
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
                 times(m, tint_rgb, lerp(m, scalar(m, "WoodsStainDepth", 0.5, -2300, -1000), scalar(m, "WoodsPurpleDepth", 1.15, -2300, -940), purple, -2200, -970), -2100, -1000),
                 # (round 31: the trees of a blue or violet country beyond Europe take no wash, or its creek lines read as channels of water)
                 times(m, times(m, times(m, stain, scalar(m, "WoodsStain", 0.5, -2300, -1080), -2100, -1080), cool, -1950, -1080),
                       # (but the canopy of the tropics still takes it: a jungle country that took none, as Colombia, lost its
                       # colour altogether, and every tile is a shade of its holder's)
                       one_in(m, unreal.MaterialExpressionOneMinus, times(m, cool_paint, one_in(m, unreal.MaterialExpressionOneMinus, tropic, -2050, -1180), -1950, -1140), -1850, -1140), -1850, -1100), -1650, -950)
    woods = lerp(m, woods, times(m, tint_rgb, scalar(m, "WoodsPurpleDepth", 1.15, -1700, -880), -1600, -880), times(m, purple, scalar(m, "WoodsPurpleShare", 0.84, -1700, -820), -1600, -820), -1500, -900)
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
    fog = times(m, (fog_picture, "R"), scalar(m, "FogStrength", 0.92, -2300, -1700), -2100, -1800)
    sheeted = node(m, unreal.MaterialExpressionMax, -1900, -1800)
    connect(paper, "", sheeted, "A")
    connect(fog, "", sheeted, "B")
    unseen = times(m, fog, one_in(m, unreal.MaterialExpressionOneMinus, paper, -2100, -1650), -1900, -1650)  # fogged, and not yet the staff map
    hill = one_in(m, unreal.MaterialExpressionSaturate,
                  plus(m, scalar(m, "PaperShade", 0.6, -600, -1000), times(m, (relief, "B"), scalar(m, "PaperSun", 0.92, -600, -900), -450, -950), -300, -950), -150, -950)
    ground_paper = lerp(m, vector(m, "PaperColour", (0.74, 0.66, 0.5, 1.0), -300, -1300), vector(m, "FogColour", (0.25, 0.215, 0.145, 1.0), -300, -1400), unseen, -150, -1350)
    sheet = lerp(m, vector(m, "PaperInk", (0.2, 0.135, 0.075, 1.0), -300, -1150), ground_paper, hill, 0, -1100)
    wash_p = lerp(m, scalar(m, "PaperWash", 0.5, -600, -1450), scalar(m, "FogWash", 0.18, -600, -1520), fog, -500, -1480)
    tinted = one_in(m, unreal.MaterialExpressionSaturate,
                    plus(m, times(m, wash_p, plus(m, boost, 1.0, -600, -1380), -450, -1420),
                         times(m, (tint, "A"), scalar(m, "PaperBand", 0.62, -600, -1620), -450, -1620), -300, -1480), -150, -1480)
    stain_p = times(m, tint_rgb, plus(m, times(m, hill, 0.5, 0, -1380), 0.6, 100, -1380), 150, -1300)
    sheet = lerp(m, sheet, stain_p, tinted, 250, -1200)
    # (the ruling: lines that slant across the table, a little under two map units apart, in the holder's colour)
    hatch_place = world_uv(m, "HatchTiling", 0.006, -1600, -2100)
    hatch_east = node(m, unreal.MaterialExpressionComponentMask, -1100, -2150, r=True, g=False, b=False, a=False)
    connect(hatch_place, "", hatch_east, "")
    hatch_south = node(m, unreal.MaterialExpressionComponentMask, -1100, -2050, r=False, g=True, b=False, a=False)
    connect(hatch_place, "", hatch_south, "")
    across = one_in(m, unreal.MaterialExpressionFrac, plus(m, hatch_east, hatch_south, -950, -2100), -800, -2100)
    middle = one_in(m, unreal.MaterialExpressionAbs, plus(m, across, -0.5, -650, -2100), -500, -2100)
    ruled = one_in(m, unreal.MaterialExpressionSaturate,
                   plus(m, times(m, middle, scalar(m, "HatchSharp", -9.0, -500, -1950), -350, -2100), 1.0, -200, -2100), -50, -2100)
    sheet = lerp(m, sheet, times(m, (tint, "RGB"), scalar(m, "HatchInk", 0.8, 100, -1950), 250, -2000),
                 times(m, times(m, ruled, fog, 100, -2100), scalar(m, "Hatch", 0.7, 100, -2200), 250, -2150), 400, -1500)
    # (and on the paper the waters are drawn in a blue-grey ink)
    sheet = lerp(m, sheet, vector(m, "PaperWater", (0.13, 0.19, 0.27, 1.0), 300, -1750), times(m, waters, scalar(m, "PaperWaters", 0.75, 300, -1650), 450, -1700), 500, -1500)
    c = lerp(m, c, sheet, sheeted, 550, -600)
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
    colour = lerp(m, colour, sheet, paper, 250, -500)
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
# Set TC_REBUILD=all to remake everything, or to a material's name (or several, with commas between) to remake those.
import os
REBUILD = os.environ.get("TC_REBUILD", "")
for name, build in (("M_MapSurface", build_map_surface), ("M_Solid", build_solid), ("M_Highlight", build_highlight), ("M_Flag", build_flag), ("M_Land", build_land), ("M_Sea", build_sea), ("M_Wake", build_wake)):
    if REBUILD == "all" or name in REBUILD.split(",") or not eal.does_asset_exist(f"{MATERIALS}/{name}"):
        build()
    else:
        unreal.log(f"Theater Command: {MATERIALS}/{name} already exists, left alone")
build_level()
unreal.log("Theater Command: content built")
