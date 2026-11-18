"""A strip of stills of one burst, for the art director: the frames the sheet asks for, laid side by side.

    python strip.py <out folder> [size=how105] [ground=earth] [frames=0,1,2,4,8,15,30,90,300] [sun=day|dusk]
                    [dist=25] [surf] [WxHxSPP=960x540x16] [seed=3]

The stage is the land team's battle field (works/battle_set.py), built once and kept as a .blend beside the output.
The camera stands at the battle camera's height, looking at a point 25 m beyond the hit (`dist`).
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import burst  # noqa: E402  (puts battle-models on the path)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
import stage  # noqa: E402
import battle_set  # noqa: E402

SUNS = {"day": dict(sun_elev=38.0, sun_turn=-35.0), "dusk": dict(sun_elev=5.5, sun_turn=150.0, strength=2.2, level=0.3)}


def arg(name, default):
    for a in sys.argv[1:]:
        if a.startswith(name + "="):
            return a.split("=", 1)[1]
    return default


def lay_stage(folder, sun):
    path = os.path.join(folder, f"stage_{sun}.blend")
    if os.path.exists(path):
        bpy.ops.wm.open_mainfile(filepath=path)
        return battle_set.Stage(bpy.data.objects["terrain"])
    kit.fresh(30)
    st = battle_set.build(seed=3)
    stage.sky(**SUNS[sun])
    bpy.ops.wm.save_as_mainfile(filepath=path)
    return st


GROUND_LOOK = {"sand": ("C9B27E", "B39A66"), "snow": ("E9EDF2", "D5DCE5"), "mud": ("4A3D2E", "5C4A36"), "town": ("8F8A80", "77736B"), "woods": ("4E5A30", "5E5A3A")}


def retint(ground):
    """The stage's field in another ground's colours (a plain mottled ground: enough to judge a burst against)."""
    a, b = GROUND_LOOK[ground]
    m = bpy.data.materials.new("ground_" + ground)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 0.12
    nz.inputs["Detail"].default_value = 10.0
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    r = nt.nodes.new("ShaderNodeValToRGB")
    r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = 0.35, (*kit.hex_rgb(a), 1)
    r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = 0.65, (*kit.hex_rgb(b), 1)
    nt.links.new(nz.outputs["Fac"], r.inputs["Fac"])
    nt.links.new(r.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8 if ground == "mud" else 0.95      # (mud matt: wet earth, not a glaze)
    m = battle_set.haze(m)
    ter = bpy.data.objects["terrain"]
    ter.data.materials.clear()
    ter.data.materials.append(m)
    for ob in list(bpy.data.objects):
        if ob.name.startswith("grass") or ob.name.startswith("tuft"):
            ob.hide_render = ground in ("snow", "sand", "mud", "town")


def S_men(size):
    return max(1.0, burst.RECIPE["sizes"][size]["scale"] ** 0.6)


def main():
    out = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else sys.argv[1]
    os.makedirs(out, exist_ok=True)
    size, ground = arg("size", "how105"), arg("ground", "earth")
    frames = [int(f) for f in arg("frames", "0,1,2,4,8,15,30,90,300").split(",")]
    sun, dist, seed = arg("sun", "day"), float(arg("dist", "25")), int(arg("seed", "3"))
    w, h, spp = (int(v) for v in arg("res", "960x540x16").split("x"))
    st = lay_stage(arg("cache", "/home/user/fxlab"), sun)
    if ground != "earth":
        retint(ground)
    sc = stage.render_setup(w, h, spp)
    sc.cycles.volume_bounces = 0
    sc.cycles.volume_step_rate = 2.0
    F = burst.Field(height=st.height, seed=seed, wind=(1.6, 0.4, 0.0), volumes="surf" not in sys.argv)
    focus = Vector((0.0, 34.0, st.height(0.0, 34.0)))
    hit = Vector((-2.0, 8.0, 0.0))
    hit.z = st.height(hit.x, hit.y)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 35
    away = Vector((-0.35, -1.0, 0.0)).normalized()
    grow = max(0.6, burst.RECIPE["sizes"][size]["scale"]) ** 0.75
    base = hit + away * float(arg("back", "40")) * grow + Vector((0.0, 0.0, float(arg("up", "14")) * grow))
    cam.location = base
    look = hit + (Vector((0, 0, 5.0)) - away * 6.0) * grow
    cam.rotation_euler = (look - base).to_track_quat("-Z", "Y").to_euler()
    rot0 = cam.rotation_euler.copy()
    # men for scale (1.75 m), standing about the hit as a squad would
    if "nomen" not in sys.argv:
        g = __import__("random").Random(5)
        for i in range(5):
            bpy.ops.import_scene.gltf(filepath="/mnt/project-files/battle-models/land/soldier/soldier_GER_LOD1.glb")
            root = next(o for o in bpy.context.selected_objects if o.type == "ARMATURE")
            a = math.radians(200 + 28 * i + g.uniform(-8, 8))
            r = float(arg("men", "16")) * S_men(size) + g.uniform(-2, 2)
            x, y = hit.x + math.cos(a) * r, hit.y + math.sin(a) * r
            root.location = (x, y, st.height(x, y))
            root.rotation_euler = (0, 0, a + math.pi + g.uniform(-0.4, 0.4))
    t0 = -0.02
    info = burst.ground_burst(F, t0, hit, size=size, ground=ground, seed=seed, cam=cam)
    print("burst", info, "camera to hit", round((base - hit).length, 1), flush=True)
    for f in frames:
        t = f / 30.0
        F.at(t)
        off, roll = F.shake(t, base)
        cam.location = base + off
        cam.rotation_euler = rot0
        cam.rotation_euler.rotate_axis("Z", math.radians(roll))
        path = os.path.join(out, f"{size}-{ground}-{sun}-f{f:03d}.png")
        if os.path.exists(path):
            continue
        tick = time.time()
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print(f"  frame {f}: {time.time() - tick:.1f}s", flush=True)


if __name__ == "__main__":
    main()
