"""Review pictures of the Asian sets: a small piece of each set's ground laid out as a battle might draw it, seen from a
general's height. Looks only; nothing here is the game's layout.

    python vignette.py <out folder> [jungle,monsoon,dryplain] [w=1600] [h=900] [samples=32]

Each scene: the set's surfaces as tiled pictures on the ground, its models stood on it (one mesh each, the copies
sharing it), the studio sun, a camera about 30 m up looking across 150 m of ground.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import numpy as np  # noqa: E402

import kit  # noqa: E402
import scenery_io  # noqa: E402
from catalogue import build  # noqa: E402
from surfaces import SURFACES, make  # noqa: E402

SURF_DIR = os.path.join(os.path.dirname(HERE), "..", "..", "..", "art", "asia", "surfaces")
_MESH = {}


def surface_material(name, folder):
    m = bpy.data.materials.get("surf_" + name)
    if m:
        return m
    path = os.path.join(folder, name + ".png")
    if not os.path.exists(path):
        from PIL import Image
        os.makedirs(folder, exist_ok=True)
        rgba, _, _ = make(name, 512, seed=sum(map(ord, name)))
        Image.fromarray(rgba, "RGBA").save(path)
    m = bpy.data.materials.new("surf_" + name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.92
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    s = 1.0 / SURFACES[name][1]
    mp.inputs["Scale"].default_value = (s, s, s)
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(path)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], img.inputs["Vector"])
    nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.02
    nt.links.new(img.outputs["Alpha"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def patch(name, x0, x1, y0, y1, folder, z=0.0, water=False):
    """A rectangle of ground in one surface (or water)."""
    me = bpy.data.meshes.new("p")
    me.from_pydata([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("patch_" + name, me)
    bpy.context.scene.collection.objects.link(ob)
    if water:
        m = bpy.data.materials.get("water") or kit.material("water", kit.hex_rgb("8C9C90"), rough=0.08, spec=0.6)
    else:
        m = surface_material(name, folder)
    me.materials.append(m)
    return ob


def edge_patch(name, x0, x1, y_front, y_back, folder, z=0.0, wave=4.0, seed=5):
    """Ground in one surface whose front edge wanders (a forest's floor running out under the trees in tongues and
    bays, not along a ruled line)."""
    r = np.random.default_rng(seed)
    xs = np.linspace(x0, x1, 61)
    ph = r.uniform(0, 2 * math.pi, 3)
    front = y_front + wave * (0.55 * np.sin(xs / 9.0 + ph[0]) + 0.3 * np.sin(xs / 3.7 + ph[1]) + 0.15 * np.sin(xs / 1.9 + ph[2]))
    V = [(x, y, z) for x, y in zip(xs, front)] + [(x, y_back, z) for x in xs]
    n = len(xs)
    F = [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)]
    me = bpy.data.meshes.new("e")
    me.from_pydata(V, [], F)
    ob = bpy.data.objects.new("patch_" + name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(surface_material(name, folder))
    return ob


def bunds(cells, folder, h=0.25, w=0.6):
    """The low earth walls round paddies: each cell's four banks, a wall two cells share built once, each bank a
    trapezoid in section (a top half as wide as the foot), built in place so the ground picture keeps its scale."""
    V, F = [], []
    lo, hi = w / 2, w / 4
    rows, cols = set(), set()
    for x0, x1, y0, y1 in cells:
        rows.update({(round(y0, 2), round(x0, 2), round(x1, 2)), (round(y1, 2), round(x0, 2), round(x1, 2))})
        cols.update({(round(x0, 2), round(y0, 2), round(y1, 2)), (round(x1, 2), round(y0, 2), round(y1, 2))})

    def bank(a, b, along_x, c):
        k = len(V)
        for t in (a, b):
            for off, z in ((-lo, -0.05), (-hi, h), (hi, h), (lo, -0.05)):
                V.append((t, c + off, z) if along_x else (c + off, t, z))
        if along_x:
            F.extend([(k, k + 4, k + 5, k + 1), (k + 1, k + 5, k + 6, k + 2), (k + 2, k + 6, k + 7, k + 3)])
        else:
            F.extend([(k, k + 1, k + 5, k + 4), (k + 1, k + 2, k + 6, k + 5), (k + 2, k + 3, k + 7, k + 6)])

    runs = []                                        # (a row's banks that meet end to end are one bank: no doubled faces)
    for y, a, b in sorted(rows):
        if runs and runs[-1][0] == y and abs(runs[-1][2] - a) < 1e-6:
            runs[-1][2] = b
        else:
            runs.append([y, a, b])
    for y, a, b in runs:
        bank(a - lo, b + lo, True, y)
    for x, a, b in sorted(cols):
        bank(a + hi, b - hi, False, x)
    me = bpy.data.meshes.new("bunds")
    me.from_pydata(V, [], F)
    me.validate()
    me.update()
    ob = bpy.data.objects.new("bunds", me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(surface_material("laterite_grass", folder))
    return ob


def place(name, x, y, rng, scale=(0.85, 1.15), lod=0, turn=None):
    key = (name, lod)
    if key not in _MESH:
        geo = build(name, lod)
        ob = scenery_io.to_blender(geo, f"{name}_src{lod}")
        ob.hide_render = True
        ob.hide_viewport = True
        _MESH[key] = ob
    src = _MESH[key]
    ob = bpy.data.objects.new(name, src.data)
    bpy.context.scene.collection.objects.link(ob)
    for md in src.modifiers:
        m2 = ob.modifiers.new(md.name, md.type)
        if md.type == "NODES":
            m2.node_group = md.node_group
    s = rng.uniform(*scale)
    ob.location = (x, y, 0.0)
    ob.rotation_euler = (0, 0, rng.uniform(0, 2 * math.pi) if turn is None else turn)
    ob.scale = (s, s, s)
    return ob


def scatter(name, n, box, rng, gap=2.0, lod=0, scale=(0.85, 1.15), avoid=(), taken=None):
    taken = taken if taken is not None else []
    x0, x1, y0, y1 = box
    tries = 0
    k = 0
    while k < n and tries < n * 40:
        tries += 1
        x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        if any(a[0] <= x <= a[1] and a[2] <= y <= a[3] for a in avoid):
            continue
        if any((x - px) ** 2 + (y - py) ** 2 < (gap + pg) ** 2 / 4 for px, py, pg in taken):
            continue
        d = math.hypot(x, y + 40)                    # (finer meshes near the camera)
        place(name, x, y, rng, scale, lod=lod if d < 70 else max(lod, 1))
        taken.append((x, y, gap))
        k += 1
    return taken


def jungle(folder, rng):
    """A village of houses on posts at the jungle's edge, a red track through it, rice behind, the forest beyond."""
    patch("laterite_grass", -120, 120, -120, 120, folder, z=-0.02)
    patch("laterite", -3, 3, -120, 120, folder)
    edge_patch("jungle_floor", -120, 120, 36, 120, folder, z=-0.01, wave=5.0)
    taken = []
    for x, y, t in ((-9, -12, 0.0), (10, -6, math.pi), (-11, 6, 0.1), (12, 12, math.pi + 0.15), (-14, 22, -0.1)):
        place("StiltHouse" if (x + y) % 3 else "StiltHouseTin", x, y, rng, (1.0, 1.0), turn=t + math.pi / 2 * (1 if x < 0 else -1))
        taken.append((x, y, 12))
    place("Shrine", 26, 30, rng, (0.7, 0.7))
    taken.append((26, 30, 14))
    road = (-4, 4, -120, 120)
    scatter("CoconutPalm", 7, (-30, 30, -25, 30), rng, 8, taken=taken, avoid=[road])
    scatter("CoconutPalmTall", 3, (-30, 30, -25, 30), rng, 8, taken=taken, avoid=[road])
    scatter("Banana", 16, (-28, 28, -22, 28), rng, 3, taken=taken, avoid=[road])
    scatter("ElephantGrass", 45, (-60, 60, -45, 32), rng, 2.0, taken=taken, avoid=[road, (-25, 25, -22, 28)])
    scatter("JungleGiant", 4, (-70, 70, 45, 100), rng, 20, taken=taken)
    scatter("JungleTree", 75, (-100, 100, 36, 115), rng, 6, taken=taken, avoid=[(-3, 3, 30, 60)])
    scatter("Bamboo", 12, (-90, 90, 30, 62), rng, 6, taken=taken, avoid=[(-3, 3, 30, 60)])
    scatter("ElephantGrass", 30, (-90, 90, 28, 40), rng, 2.0, taken=taken, avoid=[(-3, 3, 20, 60)])
    scatter("Undergrowth", 150, (-95, 95, 32, 100), rng, 1.6, taken=taken, avoid=[(-2.5, 2.5, 20, 120)])
    scatter("RockOutcrop", 1, (35, 55, -20, 0), rng, 10, taken=taken)
    return (0, -62, 26), (0, 18, 4)


def monsoon(folder, rng):
    """Rice country: paddies a third flooded behind their bunds, a farmyard and houses among willows and bamboo, the
    tiered tower over the town behind, pine on the hill."""
    patch("laterite_grass", -150, 150, -150, 150, folder, z=-0.02)
    patch("laterite", -2.5, 2.5, -150, 150, folder)
    xs = [-62, -44, -28, -12]
    ys = [-40, -26, -12, 2]
    cells = []
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            for side in (1, -1):
                a, b = sorted((side * (xs[i] + 8), side * (xs[i + 1] + 8)))
                if b < 3 and a > -3:
                    continue
                flooded = rng.random() < 0.4
                patch("paddy_mud", a, b, ys[j], ys[j + 1], folder, z=-0.01)
                cells.append((a, b, ys[j], ys[j + 1]))
                if flooded:
                    patch("water", a + 0.3, b - 0.3, ys[j] + 0.3, ys[j + 1] - 0.3, folder, z=0.04, water=True)
                else:
                    for x in np.arange(a + 1.2, b - 0.8, 2.0):
                        for y in np.arange(ys[j] + 1.2, ys[j + 1] - 0.8, 2.0):
                            d = math.hypot(x, y + 40)
                            place("RicePatch", x, y, rng, (1.0, 1.0), lod=0 if d < 35 else 1, turn=0.0)
    bunds(cells, folder)
    place("Farmyard", -16, 26, rng, (1, 1), turn=0.0)
    place("ChinaHouse", 18, 20, rng, (1, 1), turn=math.pi / 2)
    place("ThatchHouse", 16, 34, rng, (1, 1), turn=math.pi)
    place("TieredTower", 40, 85, rng, (1, 1))
    taken = [(-16, 26, 24), (18, 20, 12), (16, 34, 10), (40, 85, 10)]
    scatter("Willow", 8, (-40, 40, 6, 48), rng, 8, taken=taken, avoid=[(-3, 3, -150, 150)])
    scatter("Bamboo", 6, (-50, 50, 30, 60), rng, 8, taken=taken, avoid=[(-3, 3, -150, 150)])
    scatter("GraveMound", 6, (25, 60, -40, 5), rng, 4, taken=taken)
    scatter("StrawStack", 3, (-35, -20, 6, 14), rng, 4, taken=taken)
    scatter("HillPine", 26, (-100, 100, 62, 130), rng, 8, taken=taken)
    scatter("Boulder", 4, (-80, 80, 60, 90), rng, 6, taken=taken)
    return (0, -66, 24), (0, 14, 3)


def dryplain(folder, rng):
    """North China in late summer: loess fields ploughed and in gaoliang, a village of flat-roofed mud houses round a
    brick farmyard, poplars along the sunken road, grave mounds, a straw stack, a dry outcrop on the rise."""
    patch("loess", -150, 150, -150, 150, folder, z=-0.02)
    patch("loess_furrows", -60, -6, -45, -8, folder, z=-0.01)
    patch("loess_furrows", 6, 45, -50, -15, folder, z=-0.01)
    for x in np.arange(10, 74, 6.0):
        for y in np.arange(-12.0, 4.0, 6.0):
            place("Gaoliang", x, y, rng, (1.0, 1.0), lod=0 if x < 46 else 1, turn=0.0)
    for y in np.arange(-120, 120, 9.0):
        for x in (-4.0, 4.0):
            if -10 < y < 40:
                continue
            place("Poplar", x + rng.normal() * 0.2, y, rng, (0.85, 1.1), lod=0 if abs(y + 40) < 50 else 1)
    place("Farmyard", -20, 24, rng, (1, 1), turn=0.0)
    place("NorthHouse", 16, 22, rng, (1, 1), turn=0.0)
    place("NorthHouse", 22, 36, rng, (1, 1), turn=math.pi / 2)
    place("NorthHouse", -14, 46, rng, (1, 1), turn=0.1)
    place("TieredTower", -55, 95, rng, (0.8, 0.8))
    taken = [(-20, 24, 26), (16, 22, 12), (22, 36, 12), (-14, 46, 12)]
    scatter("Willow", 4, (-35, 35, 10, 55), rng, 10, taken=taken, avoid=[(-6, 6, -150, 150)])
    scatter("GraveMound", 7, (-70, -10, -40, 0), rng, 5, taken=taken)
    scatter("StrawStack", 3, (-8, 8, 10, 30), rng, 4, taken=taken, avoid=[(-5, 5, -150, 150)])
    place("DryOutcrop", 55, 60, rng, (1.4, 1.4))
    scatter("DryBoulder", 5, (40, 75, 40, 80), rng, 5, taken=taken)
    return (8, -66, 24), (0, 14, 3)


SCENES = {"jungle": jungle, "monsoon": monsoon, "dryplain": dryplain}


def main():
    out = sys.argv[1]
    names = sys.argv[2].split(",") if len(sys.argv) > 2 and "=" not in sys.argv[2] else list(SCENES)
    opts = dict(a.split("=", 1) for a in sys.argv[2:] if "=" in a)
    w, h, samples = int(opts.get("w", 1600)), int(opts.get("h", 900)), int(opts.get("samples", 32))
    folder = opts.get("surfaces", "/home/user/art/asia/surfaces")
    os.makedirs(out, exist_ok=True)
    for name in names:
        _MESH.clear()
        kit.fresh()
        kit.studio(sun_elev=42, sun_turn=-50, strength=3.6, sky=0.65)
        rng = np.random.default_rng(7)
        eye, target = SCENES[name](folder, rng)
        cam = kit.camera("cam", lens=32)
        bpy.context.scene.camera = cam
        kit.look_at(cam, eye, target)
        sc = bpy.context.scene
        sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.65
        kit.render_setup(w, h, samples)
        sc.render.image_settings.file_format = "JPEG"
        sc.render.image_settings.quality = 88
        path = os.path.join(out, f"{name}.jpg")
        kit.still(path)
        print("wrote", path, flush=True)


if __name__ == "__main__":
    main()
