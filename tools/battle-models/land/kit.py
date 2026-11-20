"""The battle units' shared kit: one studio, one field, one set of cameras, and one way out to Unreal.

Every battle model (the infantry squad, the tank, and those that follow) is sculpted, rigged and animated by a Blender
script in this folder, run without Blender's window:

    python soldier.py        (with the `bpy` module installed: pip install bpy==5.0.1 under Python 3.11)
    Blender --background --factory-startup --python soldier.py      (or with Blender itself, as build_tank.sh does)

and they all come through this kit so the pictures Brian sees and the files the Unreal builder gets are made the same
way. Nothing is fetched and nothing is drawn by hand: each model is made from numbers.

CONVENTIONS (the Unreal builder relies on these; the hand-off note repeats them)
  * Metres in Blender; 1 Blender unit = 1 m = 100 Unreal units. Z up. Ground at z = 0.
  * Everything faces +X, as the game's own models do (build_tank.py, build_plane.py): x forward, z up. Blender's y
    runs to the model's LEFT (Blender is right-handed); the game's JSON has y to the RIGHT, so the JSON writer flips y.
    The .fbx is written so that Unreal reads x forward, z up as well (see export_fbx).
  * Animations are IN PLACE (no root motion): the battle moves each man and tank from the numbers. Each walking or
    driving clip states its stride, the metres one loop covers, so the game can set play rate = speed / stride * length
    and the feet do not slide.
  * One action per clip, named plainly (`idle`, `walk`, `run`, `fire_stand`, ...). Loops start and end on the same pose.
  * Materials are named by PAINT (`tunic`, `trousers`, `helmet`, `hull`, ...): the game gives each its nation's colour
    from the unit table (ruling 78f), so one model serves every country.
  * Sockets are empty objects parented to bones, named `socket_<name>` (`socket_muzzle`, `socket_turret`, ...).
"""

import math
import os
import subprocess
import sys

import addon_utils
import bpy
from mathutils import Euler, Matrix, Vector

for _m in ("cycles", "io_scene_gltf2", "io_scene_fbx"):
    try:
        addon_utils.enable(_m, default_set=True)
    except Exception:  # (already on, in a full Blender)
        pass

HERE = os.path.dirname(os.path.abspath(__file__))

# The campaign's own colours (unit-table.json, the art director's), as linear-ish sRGB hex. Cloth is the uniform, paint
# the vehicles. The studio pictures use Germany and Poland, the two armies of the first battle on the slice.
NATIONS = {
    "GER": {"cloth": "5F665C", "paint": "4E5459", "helmet": "stahl"},
    "POL": {"cloth": "7C7450", "paint": "66684A", "helmet": "round"},
    "UK": {"cloth": "8B7B52", "paint": "65704E", "helmet": "brodie"},
    "USA": {"cloth": "78724C", "paint": "565C3A", "helmet": "pot"},
    "SOV": {"cloth": "7B7748", "paint": "4F5E3E", "helmet": "soviet"},
    "FRA": {"cloth": "7A7250", "paint": "5E6A48", "helmet": "adrian"},
    "ITA": {"cloth": "73806C", "paint": "8A7F5E", "helmet": "round"},
    "JAP": {"cloth": "A08856", "paint": "988256", "helmet": "cap"},
    "_default": {"cloth": "807A56", "paint": "60664A", "helmet": "dome"},
}


def hex_rgb(h, gain=1.0):
    """'5F665C' -> linear RGB for Blender's colour sockets."""
    def lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return tuple(min(1.0, lin(int(h[i:i + 2], 16)) * gain) for i in (0, 2, 4))


# ------------------------------------------------------------------------------------------------ the scene
def fresh(fps=30):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for _m in ("cycles", "io_scene_gltf2", "io_scene_fbx"):
        try:
            addon_utils.enable(_m, default_set=True)
        except Exception:
            pass
    sc = bpy.context.scene
    sc.render.fps = fps
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    return sc


def collection(name):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(c)
    return c


def material(name, rgb, rough=0.8, metal=0.0, sheen=0.0, spec=0.4, noise=0.0, noise_scale=60.0, bump=0.0):
    """A plain principled material. `noise` mottles the colour by that much (cloth, worn paint) and `bump` roughens the
    surface: both for the studio pictures only (a game export keeps the plain colour, and the name says the paint)."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    col = (*rgb, 1.0) if len(rgb) == 3 else rgb
    bsdf.inputs["Base Color"].default_value = col
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = spec
    if sheen and "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = sheen
    if noise or bump:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = noise_scale
        nz.inputs["Detail"].default_value = 6.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        if noise:
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = noise
            mix.inputs["A"].default_value = col
            ramp = nt.nodes.new("ShaderNodeValToRGB")
            ramp.color_ramp.elements[0].color = (0.55, 0.55, 0.55, 1)
            ramp.color_ramp.elements[1].color = (1.25, 1.25, 1.25, 1)
            nt.links.new(nz.outputs["Fac"], ramp.inputs["Fac"])
            nt.links.new(ramp.outputs["Color"], mix.inputs["B"])
            nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
        if bump:
            b = nt.nodes.new("ShaderNodeBump")
            b.inputs["Strength"].default_value = bump
            b.inputs["Distance"].default_value = 0.002
            nt.links.new(nz.outputs["Fac"], b.inputs["Height"])
            nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def studio(sun_elev=38.0, sun_turn=-35.0, strength=3.2, sky=0.55, warm=True):
    """Daylight from one sun, a soft sky behind it, AgX tone. The campaign's lamp is warm, so is this."""
    sc = bpy.context.scene
    world = bpy.data.worlds.new("sky")
    sc.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    grad = nt.nodes.new("ShaderNodeTexGradient")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    # a sky that is pale and warm at the horizon and bluer overhead
    mapr = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(tc.outputs["Window"], sep.inputs[0])
    nt.links.new(sep.outputs["Y"], mapr.inputs["Value"])
    mapr.inputs["From Min"].default_value = 0.0
    mapr.inputs["From Max"].default_value = 1.0
    nt.links.new(mapr.outputs["Result"], ramp.inputs["Fac"])
    ramp.color_ramp.elements[0].color = (0.86, 0.80, 0.70, 1)
    ramp.color_ramp.elements[1].color = (0.50, 0.62, 0.78, 1)
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = sky
    sun_data = bpy.data.lights.new("sun", "SUN")
    sun_data.energy = strength
    sun_data.angle = math.radians(3.0)
    sun_data.color = (1.0, 0.93, 0.82) if warm else (1, 1, 1)
    sun = bpy.data.objects.new("sun", sun_data)
    sc.collection.objects.link(sun)
    sun.rotation_euler = Euler((math.radians(90 - sun_elev), 0.0, math.radians(sun_turn)), "XYZ")
    return sun


def field_ground(size=400.0, kind="grass"):
    """The field the pictures stand on: grass worn to earth in patches, in the campaign's soft olive and ochre."""
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0, 0, 0))
    g = bpy.context.object
    g.name = "ground"
    m = bpy.data.materials.new("ground")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    big = nt.nodes.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = 0.08
    big.inputs["Detail"].default_value = 8.0
    fine = nt.nodes.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value = 3.0
    fine.inputs["Detail"].default_value = 10.0
    nt.links.new(tc.outputs["Object"], big.inputs["Vector"])
    nt.links.new(tc.outputs["Object"], fine.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    if kind == "grass":
        e[0].position, e[0].color = 0.42, (*hex_rgb("7E8A4C"), 1)
        e[1].position, e[1].color = 0.62, (*hex_rgb("8C7B58"), 1)
    else:
        e[0].position, e[0].color = 0.42, (*hex_rgb("8E8566"), 1)
        e[1].position, e[1].color = 0.62, (*hex_rgb("6E6448"), 1)
    nt.links.new(big.outputs["Fac"], ramp.inputs["Fac"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 0.35
    nt.links.new(ramp.outputs["Color"], mix.inputs["A"])
    fr = nt.nodes.new("ShaderNodeValToRGB")
    fr.color_ramp.elements[0].color = (0.6, 0.6, 0.6, 1)
    fr.color_ramp.elements[1].color = (1.2, 1.2, 1.2, 1)
    nt.links.new(fine.outputs["Fac"], fr.inputs["Fac"])
    nt.links.new(fr.outputs["Color"], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    nt.links.new(fine.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    g.data.materials.append(m)
    return g


def backdrop(colour="D9D2C3"):
    """A plain paper-coloured sweep for model sheets, with a soft shadow catcher look."""
    bpy.ops.mesh.primitive_plane_add(size=60, location=(0, 0, 0))
    g = bpy.context.object
    g.name = "backdrop"
    g.data.materials.append(material("backdrop", hex_rgb(colour), rough=0.95))
    return g


# ------------------------------------------------------------------------------------------------ cameras
def camera(name="cam", lens=50.0):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_start = 0.05
    cd.clip_end = 2000.0
    cam = bpy.data.objects.new(name, cd)
    bpy.context.scene.collection.objects.link(cam)
    bpy.context.scene.camera = cam
    return cam


def look_at(cam, eye, target):
    cam.location = Vector(eye)
    d = Vector(target) - Vector(eye)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def orbit(cam, target, dist, pitch_deg, yaw_deg):
    """Place `cam` `dist` metres from target, looking down `pitch_deg`, from compass bearing `yaw_deg`
    (0 = from the model's front, i.e. from +X)."""
    p, y = math.radians(pitch_deg), math.radians(yaw_deg)
    t = Vector(target)
    eye = t + Vector((math.cos(y) * math.cos(p), math.sin(y) * math.cos(p), math.sin(p))) * dist
    look_at(cam, eye, t)


def turntable(cam, target, dist, pitch_deg, frames, start_yaw=-30.0, frame0=1):
    """Keyframe a full turn round the target over `frames` frames."""
    for i in range(frames + 1):
        orbit(cam, target, dist, pitch_deg, start_yaw + 360.0 * i / frames)
        cam.keyframe_insert("location", frame=frame0 + i)
        cam.keyframe_insert("rotation_euler", frame=frame0 + i)
    _linear(cam)


def _linear(obj):
    ad = obj.animation_data
    if not ad or not ad.action:
        return
    for fc in fcurves_of(ad.action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"


def fcurves_of(action):
    """Every F-curve in an action (Blender 4.4+ keeps them in layers, strips and channel bags)."""
    if hasattr(action, "fcurves"):
        try:
            return list(action.fcurves)
        except Exception:
            pass
    out = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []):
                out += list(bag.fcurves)
    return out


# ------------------------------------------------------------------------------------------------ rendering
def render_setup(w=1280, h=720, samples=48, transparent=False, fast=False):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 16 if fast else samples
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 4
    sc.cycles.diffuse_bounces = 2
    sc.cycles.glossy_bounces = 2
    sc.cycles.transparent_max_bounces = 4
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = transparent
    sc.render.image_settings.file_format = "PNG"
    try:
        sc.view_settings.view_transform = "AgX"
        sc.view_settings.look = "AgX - Medium High Contrast"
    except Exception:
        pass
    sc.render.threads_mode = "AUTO"
    return sc


def still(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return path


def frames(folder, start, end, step=1):
    """Render frames start..end into folder/0001.png ... (numbered from 1 whatever `start` is)."""
    os.makedirs(folder, exist_ok=True)
    sc = bpy.context.scene
    n = 1
    for f in range(start, end + 1, step):
        sc.frame_set(f)
        sc.render.filepath = os.path.join(folder, f"{n:04d}.png")
        bpy.ops.render.render(write_still=True)
        n += 1
    return folder


def encode(folder, out_base, fps=30, gif_width=640, gif=True):
    """folder/0001.png ... -> out_base.mp4 (H.264, plays everywhere) and out_base.gif (small, loops in a chat)."""
    os.makedirs(os.path.dirname(out_base), exist_ok=True)
    src = os.path.join(folder, "%04d.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(fps), "-i", src, "-c:v", "libx264",
                    "-pix_fmt", "yuv420p", "-crf", "18", "-movflags", "+faststart", out_base + ".mp4"], check=True)
    if gif:
        pal = out_base + ".palette.png"
        vf = f"fps={min(fps, 20)},scale={gif_width}:-1:flags=lanczos"
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(fps), "-i", src, "-vf", vf + ",palettegen=stats_mode=diff", pal], check=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(fps), "-i", src, "-i", pal, "-lavfi",
                        vf + " [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=4", "-loop", "0", out_base + ".gif"], check=True)
        os.remove(pal)
    return out_base + ".mp4"


# ------------------------------------------------------------------------------------------------ export
def _select_only(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    if objs:
        bpy.context.view_layer.objects.active = objs[0]


def export_glb(path, objs):
    """glTF binary with every action as its own clip (the Unreal Interchange importer reads it; so does any viewer)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    _select_only(objs)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_force_sampling=True, export_frame_step=1,
                              export_def_bones=False, export_apply=True, export_yup=True, export_skins=True,
                              export_morph=False, export_extras=True)
    return path


def export_fbx(path, objs):
    """FBX for Unreal's classic importer: metres scaled to centimetres, no leaf bones, every action a take."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    _select_only(objs)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                             object_types={"ARMATURE", "MESH", "EMPTY"}, use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=True, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, axis_forward="X", axis_up="Z")
    return path


def triangles(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    n = 0
    for o in objs:
        if o.type != "MESH":
            continue
        me = o.evaluated_get(dg).to_mesh()
        me.calc_loop_triangles()
        n += len(me.loop_triangles)
        o.evaluated_get(dg).to_mesh_clear()
    return n


# ------------------------------------------------------------------------------------------------ sheets
def sheet(images, out, cols=3, labels=None, pad=16, bg=(232, 226, 214), label_h=34, title=None):
    """Lay pictures out in a grid on paper, each with its label beneath, for Brian."""
    from PIL import Image, ImageDraw, ImageFont
    ims = [Image.open(p).convert("RGB") for p in images]
    w = max(i.width for i in ims)
    h = max(i.height for i in ims)
    rows = (len(ims) + cols - 1) // cols
    top = 56 if title else 0
    W = cols * w + (cols + 1) * pad
    H = top + rows * (h + label_h) + (rows + 1) * pad
    S = Image.new("RGB", (W, H), bg)
    d = ImageDraw.Draw(S)
    font = _font(22)
    tfont = _font(30)
    if title:
        d.text((pad, 14), title, fill=(40, 34, 28), font=tfont)
    for k, im in enumerate(ims):
        r, c = divmod(k, cols)
        x = pad + c * (w + pad)
        y = top + pad + r * (h + label_h + pad)
        S.paste(im, (x, y))
        if labels:
            d.text((x + 4, y + h + 6), labels[k], fill=(40, 34, 28), font=font)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    S.save(out)
    return out


def _font(size):
    from PIL import ImageFont
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def args():
    """Arguments after `--` when run by Blender, or after the script when run by Python."""
    a = sys.argv
    return a[a.index("--") + 1:] if "--" in a else a[1:]
